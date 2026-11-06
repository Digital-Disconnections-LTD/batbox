#include "auth/oauth_listener/IndigenousServer.hpp"
#include "runtime/JsonValue.hpp"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
void require(bool value,const char* message){
  if(!value)throw std::runtime_error(message);
}
}
int main(){
  using namespace batbox;
  event_loop::Loop loop;
  auto server=auth::oauth_listener::makeIndigenousCallbackServer(loop);
  std::string requestedUrl;
  server->onError([](const std::string& message){
    throw std::runtime_error("Callback server error: "+message);});
  auto hold=std::make_shared<event_loop::Loop::KeepAlive>(loop.hold());
  server->onRequest([&](const auth::oauth_listener::Request& request){
    requestedUrl=request.url;
    request.response->writeHead(200,Json{{"Content-Type","text/html"}});
    request.response->end("<h1>Authentication Successful</h1>");
    hold->release();
    loop.stop();
  });
  int port=0;
  bool started=false;
  server->start(std::nullopt).then([&](const int* number,
      const std::exception_ptr& error){
    if(error)std::rethrow_exception(error);
    port=*number;started=true;
  });
  for(int i=0;i<20&&!started;++i)loop.runReady();
  require(started&&port>0,"callback server did not start");
  std::string response;
  std::thread browser([&]{
    const int socket=::socket(AF_INET,SOCK_STREAM,0);
    if(socket<0)throw std::runtime_error("Browser socket failed");
    sockaddr_in address{};
    address.sin_family=AF_INET;
    address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    address.sin_port=htons(static_cast<unsigned short>(port));
    if(::connect(socket,reinterpret_cast<sockaddr*>(&address),
        sizeof(address))<0)
      throw std::runtime_error("Browser connect failed");
    const std::string request="GET /callback?code=native-code&state="
      "expected-state HTTP/1.1\r\nHost: localhost\r\nConnection: close"
      "\r\n\r\n";
    if(::send(socket,request.data(),request.size(),0)<0)
      throw std::runtime_error("Browser send failed");
    char bytes[4096];
    for(;;){
      const auto count=::recv(socket,bytes,sizeof(bytes),0);
      if(count<=0)break;
      response.append(bytes,static_cast<std::size_t>(count));
    }
    ::close(socket);
  });
  loop.run();
  browser.join();
  server->removeAllListeners();
  server->close();
  require(requestedUrl=="/callback?code=native-code&state=expected-state",
    "callback request URL was not delivered");
  require(response.starts_with("HTTP/1.1 200 OK\r\n"),"callback status line");
  require(response.find("Content-Type: text/html\r\n")!=std::string::npos,
    "callback response header");
  require(response.find("<h1>Authentication Successful</h1>")!=
    std::string::npos,"callback response body");
  std::cout<<"native OAuth callback server passed\n";
}
