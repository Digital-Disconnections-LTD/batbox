#include "client_factory/ClientFactory.hpp"
#include "runtime/JsonValue.hpp"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
int listener(unsigned short& port){
  const int server=socket(AF_INET,SOCK_STREAM,0);
  if(server<0)throw std::runtime_error("socket failed");
  int reuse=1;
  setsockopt(server,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
  sockaddr_in address{};
  address.sin_family=AF_INET;
  address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
  if(bind(server,reinterpret_cast<sockaddr*>(&address),sizeof(address))<0||
      listen(server,1)<0)throw std::runtime_error("listen failed");
  socklen_t size=sizeof(address);
  if(getsockname(server,reinterpret_cast<sockaddr*>(&address),&size)<0)
    throw std::runtime_error("getsockname failed");
  port=ntohs(address.sin_port);
  return server;
}
std::string readRequest(int client){
  timeval timeout{4,0};
  setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
  std::string request;
  char buffer[4096];
  for(;;){
    const auto size=recv(client,buffer,sizeof(buffer),0);
    if(size<=0)throw std::runtime_error("request receive failed");
    request.append(buffer,static_cast<std::size_t>(size));
    const auto body=request.find("\r\n\r\n");
    if(body==std::string::npos)continue;
    const auto key=request.find("Content-Length:");
    const auto lower=request.find("content-length:");
    const auto at=key!=std::string::npos?key:lower;
    if(at==std::string::npos)throw std::runtime_error("No content length");
    const auto colon=request.find(':',at);
    const auto end=request.find("\r\n",colon);
    const auto length=std::stoul(request.substr(colon+1,end-colon-1));
    if(request.size()>=body+4+length)return request;
  }
}
}
int main(){
  using namespace batbox;
  unsigned short port=0;
  const int server=listener(port);
  std::string request;
  std::exception_ptr serverError;
  std::thread responder([&]{
    try{
      const int client=accept(server,nullptr,nullptr);
      if(client<0)throw std::runtime_error("accept failed");
      request=readRequest(client);
      const std::string body=R"({"id":"message-1","content":[]})";
      const std::string response="HTTP/1.1 200 OK\r\nContent-Type: "
        "application/json\r\nContent-Length: "+
        std::to_string(body.size())+"\r\nConnection: close\r\n\r\n"+body;
      if(send(client,response.data(),response.size(),0)<0)
        throw std::runtime_error("response send failed");
      close(client);
    }catch(...){serverError=std::current_exception();}
  });
  event_loop::Loop loop;
  client_factory::Services services;
  services.environment=[](const std::string&)->Json{return nullptr;};
  services.getUserBat=[](){return "batbox-test/1";};
  services.checkAndRefreshAuth=[&]{
    event_loop::Deferred<event_loop::Unit> result(loop);
    result.resolve({});return result;};
  services.isSubscriber=[](){return false;};
  services.isNonInteractive=[](){return false;};
  services.apiKeyHelper=[&](bool){
    event_loop::Deferred<std::optional<std::string>> result(loop);
    result.resolve("helper-token");return result;};
  services.getProxyFetchOptions=[](){return client_factory::NetworkOptions{};};
  services.getApiKey=[]()->std::optional<std::string>{return "api-key";};
  services.debugToStderr=[](){return false;};
  services.directBaseUrl="http://127.0.0.1:"+std::to_string(port);
  std::shared_ptr<transport::Client> client;
  const auto created=client_factory::getBatboxClient(loop,{},services);
  created.then([&](const std::shared_ptr<transport::Client>* value,
      const std::exception_ptr& error){
    if(error)std::rethrow_exception(error);
    client=*value;
  });
  for(int i=0;i<100&&!client;++i)loop.runReady();
  assert(client);
  const auto response=client->request(Json{{"model","qwen"},
    {"max_tokens",1},{"messages",Json::array()}},false);
  responder.join();close(server);
  if(serverError)std::rethrow_exception(serverError);
  assert(response.status==200);
  assert(response.body["id"]=="message-1");
  assert(request.find("POST /v1/messages HTTP/1.1")!=std::string::npos);
  assert(request.find("x-api-key: api-key")!=std::string::npos);
  assert(request.find("Authorization: Bearer helper-token")!=
    std::string::npos||request.find("authorization: Bearer helper-token")!=
    std::string::npos);
  assert(request.find("User-Agent: batbox-test/1")!=std::string::npos||
    request.find("user-agent: batbox-test/1")!=std::string::npos);
  const auto body=request.substr(request.find("\r\n\r\n")+4);
  assert(value::parse(body)["model"]=="qwen");
  std::cout<<"native direct client loopback passed\n";
}
