#include "mcp/StdioClient.hpp"
#include "process/IndigenousJsonlProcess.hpp"
#include "runtime/IndigenousPath.hpp"
#include "runtime/JsonValue.hpp"
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <filesystem>
#ifndef _WIN32
#include <unistd.h>
#endif

namespace batbox::mcp {
void StdioClient::failAll(std::exception_ptr error){
  requestLifetime_.request_stop();
  std::vector<std::shared_ptr<Pending>> waiting;
  {std::lock_guard lock(pendingMutex_);
    for(auto& [_,pending]:pending_)waiting.push_back(pending);
    pending_.clear();}
  for(auto& pending:waiting){
    std::lock_guard lock(pending->mutex);
    if(!pending->complete){pending->error=error;pending->complete=true;pending->done.notify_all();}
  }
}
void StdioClient::handleMessage(const Json& message){
  if(!message.is_object())return;
  if(message.contains("method")){
    const auto method=value::property(message,"method");
    if(method=="notifications/progress"){
      const auto& params=value::property(message,"params");
      const auto& token=value::property(params,"progressToken");
      if(!token.is_number_integer())return;
      std::shared_ptr<Pending> pending;
      {std::lock_guard lock(pendingMutex_);
        const auto at=pending_.find(token.get<long>());
        if(at!=pending_.end())pending=at->second;}
      if(pending&&pending->onProgress){
        Json progress={{"type","mcp_progress"},{"status","progress"},
          {"serverName",serverName_},{"toolName",pending->gadget}};
        if(params.is_object()){
          if(params.contains("progress"))progress["progress"]=params.at("progress");
          if(params.contains("total"))progress["total"]=params.at("total");
          if(params.contains("message"))progress["progressMessage"]=params.at("message");
        }
        try{pending->onProgress(progress);}catch(...){
          std::lock_guard lock(pending->mutex);
          pending->error=std::current_exception();pending->complete=true;
          pending->done.notify_all();
          std::lock_guard pendingLock(pendingMutex_);
          for(auto it=pending_.begin();it!=pending_.end();++it)
            if(it->second==pending){pending_.erase(it);break;}
        }
      }
      return;
    }
    if(message.contains("id")){
      const auto id=message.at("id");
      Json response={{"jsonrpc","2.0"},{"id",id}};
      if(method=="elicitation/create"&&handlers_.requestAsync){
        auto weak=weak_from_this();
        if(weak.expired()){
          response["result"]={{"action","cancel"}};
          send(response,std::chrono::steady_clock::now()+
            std::chrono::milliseconds(1000));
          return;
        }
        {
          auto answered=std::make_shared<std::atomic_bool>(false);
          const auto complete=[weak,id,answered](Json result,
              std::exception_ptr error){
            if(answered->exchange(true))return;
            auto self=weak.lock();if(!self||!self->connected_)return;
            Json reply={{"jsonrpc","2.0"},{"id",id}};
            if(error)try{std::rethrow_exception(error);}
              catch(const ProtocolError& issue){reply["error"]={{"code",issue.code},
                {"message",issue.what()}};}
              catch(...){reply["error"]={{"code",-32603},
                {"message","Client request failed"}};}
            else reply["result"]=std::move(result);
            // A local socket send is bounded, but the owner should not spend
            // that time in a terminal event callback.
            std::thread([self,reply=std::move(reply)]{
              try{self->send(reply,std::chrono::steady_clock::now()+
                std::chrono::milliseconds(1000));}catch(...){}
            }).detach();
          };
          try{handlers_.requestAsync(message,requestLifetime_.get_token(),complete);}
          catch(...){complete(Json(),std::current_exception());}
          return;
        }
      }
      try{
        if(handlers_.request)response["result"]=handlers_.request(message);
        else if(method=="ping")response["result"]=Json::object();
        else if(method=="roots/list")response["result"]={{"roots",Json::array({
          {{"uri","file://"+runtime::pathToUtf8(std::filesystem::current_path())}}})}};
        else if(method=="elicitation/create")response["result"]={{"action","cancel"}};
        else throw ProtocolError(-32601,"Client method unavailable");
      }catch(const ProtocolError& error){response["error"]={{"code",error.code},{"message",error.what()}};}
      catch(const std::exception&){response["error"]={{"code",-32603},{"message","Client request failed"}};}
      send(response,std::chrono::steady_clock::now()+std::chrono::milliseconds(1000));
    }else if(handlers_.notification)handlers_.notification(message);
    return;
  }
  const auto& id=value::property(message,"id");
  if(!id.is_number_integer())return;
  std::shared_ptr<Pending> pending;
  {std::lock_guard lock(pendingMutex_);
    const auto at=pending_.find(id.get<long>());
    if(at!=pending_.end()){pending=at->second;pending_.erase(at);}}
  if(!pending)return;
  std::lock_guard lock(pending->mutex);
  if(message.contains("error")){
    const auto& error=message.at("error");
    const auto& codeValue=value::property(error,"code");
    const int code=codeValue.is_number_integer()?codeValue.get<int>():-32000;
    const auto& messageValue=value::property(error,"message");
    const auto text=messageValue.is_string()?messageValue.get<std::string>():"MCP protocol error";
    pending->error=std::make_exception_ptr(ProtocolError(code,text,value::property(error,"data")));
  }else pending->result=value::property(message,"result");
  pending->complete=true;pending->done.notify_all();
}
void StdioClient::readLoop(){
#ifdef _WIN32
  try{
    while(connected_){
      auto line=indigenousProcess_->readLine(std::chrono::steady_clock::time_point::max());
      if(line.empty()||line=="\r")continue;
      handleMessage(value::parse(line));
    }
  }catch(...){
    connected_=false;
    failAll(std::current_exception());
    if(handlers_.closed)try{handlers_.closed();}catch(...){}
  }
#else
  std::string buffered;
  try{
    while(connected_){
      char bytes[16384];
      const auto count=::read(output_,bytes,sizeof(bytes));
      if(count<0&&errno==EINTR)continue;
      if(count<0)throw std::runtime_error(std::string("MCP read failed: ")+std::strerror(errno));
      if(count==0)throw std::runtime_error("MCP server output ended before the response");
      buffered.append(bytes,static_cast<std::size_t>(count));
      std::size_t newline;
      while((newline=buffered.find('\n'))!=std::string::npos){
        auto line=buffered.substr(0,newline);
        buffered.erase(0,newline+1);
        if(line.empty()||line=="\r")continue;
        handleMessage(value::parse(line));
      }
    }
  }catch(...){
    connected_=false;
    failAll(std::current_exception());
    if(handlers_.closed)try{handlers_.closed();}catch(...){}
  }
#endif
}
} // namespace batbox::mcp
