#include "lsp/Client.hpp"
#include <future>
#include <stdexcept>

namespace batbox::lsp {
Client::Client(std::string name,std::vector<std::string> command,
    std::map<std::string,std::string> env,std::string cwd,Notification notification):
  name_(std::move(name)),notification_(std::move(notification)),
  decoder_([this](Json message){receive(std::move(message));}){
  dispatcher_=std::thread([this]{dispatch();});
  try{
    process::IndigenousChildOptions options;
    options.lineMode=false;
    if(!cwd.empty())options.cwd=cwd;
    options.onData=[this](const std::string& bytes,bool isStdout){
      if(!isStdout)return;
      try{std::lock_guard lock(parserMutex_);decoder_.feed(bytes);}
      catch(...){std::lock_guard lock(queueMutex_);parserFailure_=std::current_exception();queueReady_.notify_all();}
    };
    process_=std::make_unique<process::IndigenousJsonlProcess>(command,env,std::move(options));
  }catch(...){
    {std::lock_guard lock(queueMutex_);closing_=true;}
    queueReady_.notify_all();dispatcher_.join();throw;
  }
}
Client::~Client(){stop();}
void Client::receive(Json message){
  {std::lock_guard lock(queueMutex_);queue_.push_back(std::move(message));}
  queueReady_.notify_one();
}
void Client::dispatch(){
  for(;;){
    Json message;
    {
      std::unique_lock lock(queueMutex_);
      queueReady_.wait(lock,[this]{return closing_||parserFailure_||!queue_.empty();});
      if(parserFailure_||closing_)break;
      message=std::move(queue_.front());queue_.pop_front();
    }
    try{
      if(!message.is_object())continue;
      if(message.contains("id")&&!message.contains("method")){
        int id=message.at("id").get<int>();
        std::shared_ptr<Pending> pending;
        {std::lock_guard lock(pendingMutex_);auto it=pending_.find(id);
          if(it!=pending_.end()){pending=std::move(it->second);pending_.erase(it);}}
        if(pending)pending->promise.set_value(std::move(message));
      }else if(message.contains("method")){
        auto method=message.at("method").get<std::string>();
        auto params=message.value("params",Json::object());
        if(message.contains("id")){
          Json result=nullptr;
          if(method=="workspace/configuration"){
            result=Json::array();
            if(params.is_object()&&params.value("items",Json::array()).is_array())
              for(const auto& item:params.at("items")){(void)item;result.push_back(nullptr);}
          }else if(method=="window/workDoneProgress/create"||method=="client/registerCapability"||
                   method=="client/unregisterCapability")result=nullptr;
          else if(method=="workspace/workspaceFolders")result=Json::array();
          write({{"jsonrpc","2.0"},{"id",message.at("id")},{"result",result}});
        }else if(notification_)notification_(method,params);
      }
    }catch(...){/* Bad server messages do not tear down the connection. */}
  }
  std::map<int,std::shared_ptr<Pending>> abandoned;
  {std::lock_guard lock(pendingMutex_);abandoned.swap(pending_);}
  for(auto& [id,pending]:abandoned){(void)id;
    try{pending->promise.set_exception(std::make_exception_ptr(
      std::runtime_error("LSP server disconnected")));}catch(...){}
  }
}
void Client::write(const Json& message){
  std::lock_guard lock(writeMutex_);
  if(!process_)throw std::runtime_error("LSP server disconnected");
  process_->writeLine(frame(message),std::chrono::steady_clock::now()+std::chrono::seconds(5));
}
Json Client::request(const std::string& method,const Json& params,
    std::chrono::milliseconds timeout,std::stop_token signal){
  auto pending=std::make_shared<Pending>();
  auto future=pending->promise.get_future();
  int id;
  {std::lock_guard lock(pendingMutex_);id=nextId_++;pending_[id]=pending;}
  try{write({{"jsonrpc","2.0"},{"id",id},{"method",method},{"params",params}});}
  catch(...){std::lock_guard lock(pendingMutex_);pending_.erase(id);throw;}
  auto deadline=std::chrono::steady_clock::now()+timeout;
  while(future.wait_for(std::chrono::milliseconds(25))!=std::future_status::ready){
    if(signal.stop_requested()||std::chrono::steady_clock::now()>=deadline){
      {std::lock_guard lock(pendingMutex_);pending_.erase(id);}
      try{write({{"jsonrpc","2.0"},{"method","$/cancelRequest"},{"params",{{"id",id}}}});}catch(...){}
      throw std::runtime_error(signal.stop_requested()?"LSP request cancelled":"LSP request timed out");
    }
    {std::lock_guard lock(queueMutex_);if(parserFailure_)std::rethrow_exception(parserFailure_);}
  }
  auto reply=future.get();
  if(reply.contains("error")&&!reply.at("error").is_null()){
    const auto& error=reply.at("error");
    auto message=error.value("message",std::string("LSP request error"));
    int code=error.value("code",0);
    throw std::runtime_error("LSP error "+std::to_string(code)+": "+message);
  }
  return reply.value("result",Json());
}
void Client::notify(const std::string& method,const Json& params){
  write({{"jsonrpc","2.0"},{"method",method},{"params",params}});
}
void Client::stop()noexcept{
  if(process_){
    try{
      if(initialized_)request("shutdown",nullptr,std::chrono::milliseconds(1200));
      notify("exit",nullptr);
    }catch(...){process_->kill();}
  }
  {std::lock_guard lock(queueMutex_);closing_=true;}
  queueReady_.notify_all();
  if(dispatcher_.joinable())dispatcher_.join();
  if(process_){
    process_->closeStdin();
    try{process_->waitForExit(std::chrono::steady_clock::now()+std::chrono::milliseconds(800));}
    catch(...){process_->kill();}
    process_->close();process_.reset();
  }
}
long Client::pid()const{return process_?process_->pid():0;}
}
