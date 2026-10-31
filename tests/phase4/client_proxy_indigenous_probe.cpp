#include "client_factory/ClientFactory.hpp"
#include "runtime/JsonValue.hpp"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
using namespace batbox;
struct Server {
  int fd=-1;
  std::string path;
  Server(int value,std::string name):fd(value),path(std::move(name)){}
  Server(const Server&)=delete;
  Server& operator=(const Server&)=delete;
  Server(Server&& other)noexcept:fd(other.fd),path(std::move(other.path)){
    other.fd=-1;other.path.clear();
  }
  ~Server(){if(fd>=0)close(fd);if(!path.empty())unlink(path.c_str());}
};
Server tcp(unsigned short& port){
  Server server{socket(AF_INET,SOCK_STREAM,0),{}};
  if(server.fd<0)throw std::runtime_error("socket failed");
  sockaddr_in address{};
  address.sin_family=AF_INET;
  address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
  if(bind(server.fd,reinterpret_cast<sockaddr*>(&address),
      sizeof(address))<0||listen(server.fd,1)<0)
    throw std::runtime_error("TCP listen failed");
  socklen_t length=sizeof(address);
  getsockname(server.fd,reinterpret_cast<sockaddr*>(&address),&length);
  port=ntohs(address.sin_port);
  return server;
}
Server unixSocket(){
  Server server{socket(AF_UNIX,SOCK_STREAM,0),
    "/tmp/batbox-client-proxy-"+std::to_string(getpid())+".sock"};
  if(server.fd<0)throw std::runtime_error("Unix socket failed");
  sockaddr_un address{};
  address.sun_family=AF_UNIX;
  std::strncpy(address.sun_path,server.path.c_str(),
    sizeof(address.sun_path)-1);
  unlink(server.path.c_str());
  if(bind(server.fd,reinterpret_cast<sockaddr*>(&address),
      sizeof(address))<0||listen(server.fd,1)<0)
    throw std::runtime_error("Unix listen failed");
  return server;
}
std::string answer(int fd){
  const int client=accept(fd,nullptr,nullptr);
  if(client<0)throw std::runtime_error("accept failed");
  std::string request;
  char buffer[4096];
  while(request.find("\r\n\r\n")==std::string::npos){
    const auto count=recv(client,buffer,sizeof(buffer),0);
    if(count<=0)throw std::runtime_error("receive failed");
    request.append(buffer,static_cast<std::size_t>(count));
  }
  const std::string body=R"({"id":"proxied"})";
  const auto response="HTTP/1.1 200 OK\r\nContent-Type: "
    "application/json\r\nContent-Length: "+
    std::to_string(body.size())+"\r\nConnection: close\r\n\r\n"+body;
  if(send(client,response.data(),response.size(),0)<0)
    throw std::runtime_error("send failed");
  close(client);
  return request;
}
void exercise(Server& server,const std::string& baseUrl,
    const Json& network,const std::string& requestPrefix){
  std::string wire;
  std::exception_ptr error;
  std::thread responder([&]{try{wire=answer(server.fd);}
    catch(...){error=std::current_exception();}});
  client_factory::ClientPlan plan;
  plan.proxyFetchOptions=network;
  client_factory::Services services;
  services.directBaseUrl=baseUrl;
  auto client=client_factory::makeIndigenousDirectClient(plan,services);
  const auto response=client->request(Json{{"model","qwen"},
    {"max_tokens",1},{"messages",Json::array()}},false);
  responder.join();
  if(error)std::rethrow_exception(error);
  assert(response.body["id"]=="proxied");
  assert(wire.starts_with(requestPrefix));
}
}
int main(){
  setenv("NO_PROXY","",1);
  setenv("no_proxy","",1);
  unsigned short port=0;
  auto proxy=tcp(port);
  exercise(proxy,"http://backend.invalid",
    Json{{"proxy","http://127.0.0.1:"+std::to_string(port)},
      {"keepalive",false}},
    "POST http://backend.invalid/v1/messages HTTP/1.1");
  auto socket=unixSocket();
  exercise(socket,"http://backend.local",
    Json{{"unix",socket.path}},
    "POST /v1/messages HTTP/1.1");
  std::cout<<"native proxy and Unix socket requests passed\n";
}
