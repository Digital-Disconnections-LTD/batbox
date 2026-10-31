#include "runtime/AsyncLaunch.hpp"
#include "auth/oauth_listener/IndigenousServer.hpp"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <thread>
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
void sendAll(int socket,const std::string& bytes){
  std::size_t offset=0;
  while(offset<bytes.size()){
    const auto count=::send(socket,bytes.data()+offset,
      bytes.size()-offset,MSG_NOSIGNAL);
    if(count<0&&errno==EINTR)continue;
    if(count<=0)throw std::runtime_error("OAuth callback write failed");
    offset+=static_cast<std::size_t>(count);
  }
}
class IndigenousResponse final:public Response {
  int socket_=-1,status_=200;
  Json headers_=Json::object();
  bool ended_=false;
public:
  explicit IndigenousResponse(int socket):socket_(socket){}
  ~IndigenousResponse()override{if(socket_>=0)::close(socket_);}
  void writeHead(int status,const Json& headers)override{
    if(ended_)throw std::logic_error("OAuth callback already ended");
    status_=status;headers_=headers;
  }
  void end(const std::string& body)override{
    if(ended_)return;
    ended_=true;
    std::string output="HTTP/1.1 "+std::to_string(status_)+" "+
      reason(status_)+"\r\n";
    if(headers_.is_object())for(auto it=headers_.begin();
        it!=headers_.end();++it)
      if(it.value().is_string())output+=it.key()+": "+
        it.value().get<std::string>()+"\r\n";
    output+="Content-Length: "+std::to_string(body.size())+
      "\r\nConnection: close\r\n\r\n"+body;
    try{sendAll(socket_,output);}
    catch(...){::close(socket_);socket_=-1;throw;}
    ::close(socket_);socket_=-1;
  }
};
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
      auto key=line.substr(0,colon);
      std::string lowered(key);
      for(auto& c:lowered)if(c>='A'&&c<='Z')c+=32;
      if(lowered==name){
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
std::optional<std::pair<std::string,std::string>> readRequest(int socket){
  timeval timeout{};timeout.tv_sec=10;
  ::setsockopt(socket,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
  std::string input;
  char bytes[4096];
  while(input.find("\r\n\r\n")==std::string::npos){
    const auto count=::recv(socket,bytes,sizeof(bytes),0);
    if(count<0&&errno==EINTR)continue;
    if(count<=0)return std::nullopt;
    input.append(bytes,static_cast<std::size_t>(count));
    if(input.size()>65536)return std::nullopt;
  }
  const auto firstEnd=input.find("\r\n");
  const auto first=input.substr(0,firstEnd);
  const auto firstSpace=first.find(' ');
  const auto secondSpace=firstSpace==std::string::npos?
    std::string::npos:first.find(' ',firstSpace+1);
  if(firstSpace==std::string::npos||secondSpace==std::string::npos)
    return std::nullopt;
  return std::pair{first.substr(firstSpace+1,
      secondSpace-firstSpace-1),
    headerValue(input.substr(0,input.find("\r\n\r\n")+2),"host")};
}
class IndigenousServer final:public Server,
    public std::enable_shared_from_this<IndigenousServer> {
  event_loop::Loop& loop_;
  int listener_=-1;
  std::atomic_bool closed_=false;
  std::atomic_int activeSocket_=-1;
  std::thread worker_;
  std::function<void(const Request&)> onRequest_;
  std::function<void(const std::string&)> onError_;
  void acceptLoop(int listeningSocket){
    const auto dispatcher=loop_.dispatcher();
    const auto weak=weak_from_this();
    while(!closed_){
      const int socket=::accept(listeningSocket,nullptr,nullptr);
      if(socket<0){
        if(errno==EINTR)continue;
        if(!closed_)try{dispatcher.postMission([weak,message=
            std::string(std::strerror(errno))]{
          if(const auto self=weak.lock())
            if(self->onError_)self->onError_(message);
        });}catch(...){}
        break;
      }
      activeSocket_=socket;
      auto request=readRequest(socket);
      activeSocket_=-1;
      if(closed_){::close(socket);break;}
      if(!request){::close(socket);continue;}
      auto response=std::make_shared<IndigenousResponse>(socket);
      try{dispatcher.postMission([weak,url=std::move(request->first),
          host=std::move(request->second),response]{
        if(const auto self=weak.lock()){
          if(self->onRequest_)self->onRequest_({url,host,response});
        }
      });}catch(...){break;}
    }
  }
public:
  explicit IndigenousServer(event_loop::Loop& loop):loop_(loop){}
  ~IndigenousServer()override{close();}
  event_loop::Deferred<int> start(const std::optional<int>& port)override{
    event_loop::Deferred<int> output(loop_);
    try{
      if(listener_>=0)throw std::runtime_error("Server already listening");
      if(port&&(*port<0||*port>65535))
        throw std::invalid_argument("Invalid callback port");
      listener_=::socket(AF_INET,SOCK_STREAM,0);
      if(listener_<0)throw std::runtime_error(std::strerror(errno));
      sockaddr_in address{};
      address.sin_family=AF_INET;
      address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
      address.sin_port=htons(static_cast<unsigned short>(port.value_or(0)));
      if(::bind(listener_,reinterpret_cast<sockaddr*>(&address),
          sizeof(address))<0)
        throw std::runtime_error(std::strerror(errno));
      if(::listen(listener_,16)<0)
        throw std::runtime_error(std::strerror(errno));
      socklen_t size=sizeof(address);
      if(::getsockname(listener_,reinterpret_cast<sockaddr*>(&address),
          &size)<0)
        throw std::runtime_error(std::strerror(errno));
      worker_=::batbox::runtime::contextThread([self=shared_from_this(),
          socket=listener_]{self->acceptLoop(socket);});
      output.resolve(ntohs(address.sin_port));
    }catch(...){
      if(listener_>=0){::close(listener_);listener_=-1;}
      output.reject(std::current_exception());
    }
    return output;
  }
  void onRequest(std::function<void(const Request&)> handler)override{
    onRequest_=std::move(handler);
  }
  void onError(std::function<void(const std::string&)> handler)override{
    onError_=std::move(handler);
  }
  void removeAllListeners()override{onRequest_={};onError_={};}
  void close()override{
    if(closed_.exchange(true))return;
    const int active=activeSocket_.load();
    if(active>=0)::shutdown(active,SHUT_RDWR);
    if(listener_>=0){::shutdown(listener_,SHUT_RDWR);
      ::close(listener_);listener_=-1;}
    if(worker_.joinable()){
      if(worker_.get_id()==std::this_thread::get_id())worker_.detach();
      else worker_.join();
    }
  }
};
}
std::shared_ptr<Server> makeIndigenousCallbackServer(event_loop::Loop& loop){
  return std::make_shared<IndigenousServer>(loop);
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
}
