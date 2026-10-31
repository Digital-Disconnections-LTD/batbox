#include "auth/oauth_listener/IndigenousServer.hpp"
#include <uv.h>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <deque>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>

namespace batbox::auth::oauth_listener {
namespace {
std::string reason(int code){
  switch(code){
    case 200:return "OK";
    case 201:return "Created";
    case 302:return "Found";
    case 400:return "Bad Request";
    case 404:return "Not Found";
    default:return "Response";
  }
}
std::string headerValue(std::string_view headers,std::string_view name){
  auto lineStart=headers.find("\r\n");
  if(lineStart==std::string_view::npos)return {};
  lineStart+=2;
  while(lineStart<headers.size()){
    const auto end=headers.find("\r\n",lineStart);
    if(end==std::string_view::npos)break;
    const auto line=headers.substr(lineStart,end-lineStart);
    if(line.empty())break;
    const auto colon=line.find(':');
    if(colon!=std::string_view::npos){
      std::string key(line.substr(0,colon));
      for(auto& c:key)if(c>='A'&&c<='Z')c+=32;
      if(key==name){
        auto value=line.substr(colon+1);
        while(!value.empty()&&(value.front()==' '||value.front()=='\t'))
          value.remove_prefix(1);
        return std::string(value);
      }
    }
    lineStart=end+2;
  }
  return {};
}
std::optional<std::pair<std::string,std::string>> parseRequest(
    std::string_view input){
  const auto endHeaders=input.find("\r\n\r\n");
  if(endHeaders==std::string_view::npos)return std::nullopt;
  const auto firstEnd=input.find("\r\n");
  const auto first=input.substr(0,firstEnd);
  const auto firstSpace=first.find(' ');
  const auto secondSpace=firstSpace==std::string_view::npos?
    std::string_view::npos:first.find(' ',firstSpace+1);
  if(firstSpace==std::string_view::npos||secondSpace==std::string_view::npos)
    return std::nullopt;
  return std::pair{std::string(first.substr(firstSpace+1,
      secondSpace-firstSpace-1)),
    headerValue(input.substr(0,endHeaders+2),"host")};
}
class UvServer;
class UvResponse final:public Response {
public:
  UvResponse(std::weak_ptr<UvServer> server,std::uint64_t id):
    server_(std::move(server)),id_(id){}
  ~UvResponse()override;
  void writeHead(int status,const Json& headers)override{
    if(ended_)throw std::logic_error("OAuth callback already ended");
    status_=status;headers_=headers;
  }
  void end(const std::string& body)override;
private:
  std::weak_ptr<UvServer> server_;
  std::uint64_t id_;
  int status_=200;
  Json headers_=Json::object();
  bool ended_=false;
};
class UvServer final:public Server,public std::enable_shared_from_this<UvServer>{
  struct Client {
    UvServer* owner=nullptr;
    std::uint64_t id=0;
    uv_tcp_t socket{};
    uv_timer_t timeout{};
    std::string input;
    unsigned openHandles=0;
    bool socketReady=false,timerReady=false,closing=false,writing=false;
  };
  struct Write {
    uv_write_t request{};
    std::string bytes;
    Client* client=nullptr;
  };
  struct Command {
    std::uint64_t id;
    std::optional<std::string> bytes;
  };
  event_loop::Loop& owner_;
  uv_loop_t loop_{};
  uv_tcp_t listener_{};
  uv_async_t command_{};
  bool loopReady_=false,listenerReady_=false,commandReady_=false;
  std::atomic_bool running_=false,closing_=false;
  std::thread worker_;
  std::mutex queueMutex_;
  std::deque<Command> queue_;
  std::unordered_map<std::uint64_t,std::unique_ptr<Client>> clients_;
  std::uint64_t nextClient_=1;
  std::function<void(const Request&)> onRequest_;
  std::function<void(const std::string&)> onError_;
  std::optional<event_loop::Loop::KeepAlive> keep_;
  static void closeHandle(uv_handle_t* handle){
    if(handle&&!uv_is_closing(handle))uv_close(handle,nullptr);
  }
  void report(std::string message){
    auto weak=weak_from_this();
    try{owner_.dispatcher().postMission([weak,message=std::move(message)]{
      if(auto self=weak.lock())if(self->onError_)self->onError_(message);
    });}catch(...){}
  }
  void closeClient(Client& client){
    if(client.closing)return;
    client.closing=true;
    if(client.timerReady&&!uv_is_closing(
        reinterpret_cast<uv_handle_t*>(&client.timeout))){
      uv_timer_stop(&client.timeout);
      uv_close(reinterpret_cast<uv_handle_t*>(&client.timeout),closedClientHandle);
    }
    if(client.socketReady&&!uv_is_closing(
        reinterpret_cast<uv_handle_t*>(&client.socket))){
      uv_read_stop(reinterpret_cast<uv_stream_t*>(&client.socket));
      uv_close(reinterpret_cast<uv_handle_t*>(&client.socket),closedClientHandle);
    }
    if(client.openHandles==0)clients_.erase(client.id);
  }
  static void closedClientHandle(uv_handle_t* handle){
    auto* client=static_cast<Client*>(handle->data);
    if(--client->openHandles==0)client->owner->clients_.erase(client->id);
  }
  static void allocate(uv_handle_t*,std::size_t suggested,uv_buf_t* buffer){
    const auto size=std::max<std::size_t>(suggested,4096);
    buffer->base=new char[size];buffer->len=static_cast<unsigned int>(size);
  }
  static void read(uv_stream_t* stream,ssize_t count,const uv_buf_t* buffer){
    std::unique_ptr<char[]> bytes(buffer->base);
    auto* client=static_cast<Client*>(stream->data);
    if(client->closing)return;
    if(count<0){client->owner->closeClient(*client);return;}
    if(count==0)return;
    client->input.append(buffer->base,static_cast<std::size_t>(count));
    if(client->input.size()>65536){client->owner->closeClient(*client);return;}
    if(client->input.find("\r\n\r\n")==std::string::npos)return;
    uv_read_stop(stream);uv_timer_stop(&client->timeout);
    const auto request=parseRequest(client->input);
    if(!request){client->owner->closeClient(*client);return;}
    auto self=client->owner->shared_from_this();
    const auto weak=std::weak_ptr<UvServer>(self);
    auto response=std::make_shared<UvResponse>(weak,client->id);
    try{self->owner_.dispatcher().postMission([weak,response,
        url=request->first,host=request->second]{
      if(auto owner=weak.lock()){
        try{
          if(owner->onRequest_)owner->onRequest_({url,host,response});
        }catch(const std::exception& error){
          if(owner->onError_)owner->onError_(error.what());
        }
      }
    });}catch(...){client->owner->closeClient(*client);}
  }
  static void timedOut(uv_timer_t* timer){
    auto* client=static_cast<Client*>(timer->data);
    client->owner->closeClient(*client);
  }
  static void wrote(uv_write_t* request,int status){
    std::unique_ptr<Write> pending(static_cast<Write*>(request->data));
    auto* client=pending->client;
    if(status<0)client->owner->report(
      std::string("OAuth callback write failed: ")+uv_strerror(status));
    client->owner->closeClient(*client);
  }
  void writeClient(Client& client,std::string bytes){
    if(client.closing||client.writing)return;
    client.writing=true;
    auto pending=std::make_unique<Write>();
    pending->client=&client;pending->bytes=std::move(bytes);
    pending->request.data=pending.get();
    uv_buf_t buffer=uv_buf_init(pending->bytes.data(),
      static_cast<unsigned int>(pending->bytes.size()));
    const auto code=uv_write(&pending->request,
      reinterpret_cast<uv_stream_t*>(&client.socket),&buffer,1,wrote);
    if(code<0){report(std::string("OAuth callback write failed: ")+
      uv_strerror(code));closeClient(client);}
    else (void)pending.release();
  }
  void commands(){
    std::deque<Command> queued;
    {std::lock_guard lock(queueMutex_);queued.swap(queue_);}
    for(auto& command:queued){
      const auto it=clients_.find(command.id);
      if(it==clients_.end())continue;
      if(command.bytes)writeClient(*it->second,std::move(*command.bytes));
      else closeClient(*it->second);
    }
    if(closing_){
      if(listenerReady_)closeHandle(reinterpret_cast<uv_handle_t*>(&listener_));
      if(commandReady_)closeHandle(reinterpret_cast<uv_handle_t*>(&command_));
      for(auto it=clients_.begin();it!=clients_.end();){
        auto& client=*it->second;
        ++it;
        if(!client.writing)closeClient(client);
      }
    }
  }
  static void onCommand(uv_async_t* handle){
    static_cast<UvServer*>(handle->data)->commands();
  }
  static void onConnection(uv_stream_t* listener,int status){
    auto* self=static_cast<UvServer*>(listener->data);
    if(status<0){self->report(uv_strerror(status));return;}
    const auto id=self->nextClient_++;
    auto client=std::make_unique<Client>();
    client->owner=self;client->id=id;
    auto* raw=client.get();self->clients_.emplace(id,std::move(client));
    int code=uv_tcp_init(&self->loop_,&raw->socket);
    if(code<0){self->clients_.erase(id);self->report(uv_strerror(code));return;}
    raw->socketReady=true;++raw->openHandles;raw->socket.data=raw;
    code=uv_timer_init(&self->loop_,&raw->timeout);
    if(code<0){self->report(uv_strerror(code));self->closeClient(*raw);return;}
    raw->timerReady=true;++raw->openHandles;raw->timeout.data=raw;
    code=uv_accept(listener,reinterpret_cast<uv_stream_t*>(&raw->socket));
    if(code<0){self->report(uv_strerror(code));self->closeClient(*raw);return;}
    code=uv_read_start(reinterpret_cast<uv_stream_t*>(&raw->socket),allocate,read);
    if(code<0){self->report(uv_strerror(code));self->closeClient(*raw);return;}
    (void)uv_timer_start(&raw->timeout,timedOut,10000,0);
  }
  void run(int requested,std::promise<int> ready){
    bool answered=false;
    try{
      int code=uv_loop_init(&loop_);
      if(code<0)throw std::runtime_error(uv_strerror(code));
      loopReady_=true;
      code=uv_async_init(&loop_,&command_,onCommand);
      if(code<0)throw std::runtime_error(uv_strerror(code));
      commandReady_=true;command_.data=this;
      code=uv_tcp_init(&loop_,&listener_);
      if(code<0)throw std::runtime_error(uv_strerror(code));
      listenerReady_=true;listener_.data=this;
      sockaddr_in address{};
      code=uv_ip4_addr("127.0.0.1",requested,&address);
      if(code<0)throw std::runtime_error(uv_strerror(code));
      code=uv_tcp_bind(&listener_,reinterpret_cast<const sockaddr*>(&address),0);
      if(code<0)throw std::runtime_error(uv_strerror(code));
      code=uv_listen(reinterpret_cast<uv_stream_t*>(&listener_),16,onConnection);
      if(code<0)throw std::runtime_error(uv_strerror(code));
      int length=sizeof(address);
      code=uv_tcp_getsockname(&listener_,reinterpret_cast<sockaddr*>(&address),
        &length);
      if(code<0)throw std::runtime_error(uv_strerror(code));
      running_=true;
      ready.set_value(ntohs(address.sin_port));answered=true;
      uv_run(&loop_,UV_RUN_DEFAULT);
    }catch(...){
      if(!answered)ready.set_exception(std::current_exception());
      else report("OAuth callback server loop failed");
    }
    if(listenerReady_)closeHandle(reinterpret_cast<uv_handle_t*>(&listener_));
    if(commandReady_)closeHandle(reinterpret_cast<uv_handle_t*>(&command_));
    for(auto it=clients_.begin();it!=clients_.end();){
      auto& client=*it->second;++it;closeClient(client);
    }
    if(loopReady_){
      while(uv_loop_alive(&loop_))uv_run(&loop_,UV_RUN_DEFAULT);
      while(uv_loop_close(&loop_)==UV_EBUSY)uv_run(&loop_,UV_RUN_NOWAIT);
    }
    running_=false;
  }
public:
  explicit UvServer(event_loop::Loop& owner):owner_(owner){}
  ~UvServer()override{close();}
  event_loop::Deferred<int> start(const std::optional<int>& port)override{
    event_loop::Deferred<int> result(owner_);
    if(!owner_.isOwnerThread())throw std::logic_error("OAuth server owner required");
    if(worker_.joinable()||closing_){result.reject(std::make_exception_ptr(
      std::runtime_error("Server already listening")));return result;}
    if(port&&(*port<0||*port>65535)){result.reject(std::make_exception_ptr(
      std::invalid_argument("Invalid callback port")));return result;}
    std::promise<int> ready;
    auto assigned=ready.get_future();
    try{
      keep_.emplace(owner_.hold());
      worker_=std::thread([self=shared_from_this(),requested=port.value_or(0),
          ready=std::move(ready)]()mutable{self->run(requested,std::move(ready));});
      result.resolve(assigned.get());
    }catch(...){
      if(keep_){keep_->release();keep_.reset();}
      result.reject(std::current_exception());
    }
    return result;
  }
  void onRequest(std::function<void(const Request&)> handler)override{
    onRequest_=std::move(handler);
  }
  void onError(std::function<void(const std::string&)> handler)override{
    onError_=std::move(handler);
  }
  void removeAllListeners()override{onRequest_={};onError_={};}
  void close()override{
    if(!closing_.exchange(true)&&running_)uv_async_send(&command_);
    if(worker_.joinable()){
      if(worker_.get_id()==std::this_thread::get_id())worker_.detach();
      else worker_.join();
    }
    if(keep_){keep_->release();keep_.reset();}
  }
  void queueWrite(std::uint64_t id,std::string bytes){
    if(!running_)throw std::runtime_error("OAuth callback server is closed");
    {std::lock_guard lock(queueMutex_);queue_.push_back({id,std::move(bytes)});}
    uv_async_send(&command_);
  }
  void queueClose(std::uint64_t id){
    if(!running_)return;
    {std::lock_guard lock(queueMutex_);queue_.push_back({id,std::nullopt});}
    uv_async_send(&command_);
  }
};
UvResponse::~UvResponse(){if(!ended_)if(auto server=server_.lock())
  server->queueClose(id_);}
void UvResponse::end(const std::string& body){
  if(ended_)return;
  ended_=true;
  std::string output="HTTP/1.1 "+std::to_string(status_)+" "+
    reason(status_)+"\r\n";
  if(headers_.is_object())for(auto it=headers_.begin();it!=headers_.end();++it)
    if(it.value().is_string())output+=it.key()+": "+
      it.value().get<std::string>()+"\r\n";
  output+="Content-Length: "+std::to_string(body.size())+
    "\r\nConnection: close\r\n\r\n"+body;
  if(auto server=server_.lock())server->queueWrite(id_,std::move(output));
}
} // namespace
std::shared_ptr<Server> makeIndigenousCallbackServer(event_loop::Loop& loop){
  return std::make_shared<UvServer>(loop);
}
Services makeIndigenousListenerServices(event_loop::Loop& loop,
    client_factory::oauth_config::Services config,
    std::function<void(const std::string&)> logError,
    std::string inferenceScope){
  Services result;
  result.createServer=[&loop]{return makeIndigenousCallbackServer(loop);};
  result.getConfig=[config=std::move(config)]{
    return client_factory::oauth_config::getOauthConfig(config);};
  result.logError=std::move(logError);
  result.inferenceScope=std::move(inferenceScope);
  return result;
}
} // namespace batbox::auth::oauth_listener
