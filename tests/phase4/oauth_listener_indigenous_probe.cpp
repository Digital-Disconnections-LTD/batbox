#include "auth/oauth_listener/IndigenousServer.hpp"
#include "runtime/JsonValue.hpp"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cassert>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

int main(){
  using namespace batbox;
  event_loop::Loop loop;
  client_factory::oauth_config::Services config;
  config.data.production.appSuccessUrl="http://127.0.0.1/success";
  config.data.production.consoleSuccessUrl="http://127.0.0.1/console";
  config.environment=[](const std::string&)->Json{return nullptr;};
  auto services=auth::oauth_listener::makeIndigenousListenerServices(loop,config,
    [](const std::string& message){
      throw std::runtime_error("Listener error: "+message);},
    "user:inference");
  auth::oauth_listener::AuthCodeListener listener(loop,services);
  int port=0;
  bool started=false;
  listener.start().then([&](const int* number,
      const std::exception_ptr& error){
    if(error)std::rethrow_exception(error);
    port=*number;started=true;
  });
  for(int i=0;i<20&&!started;++i)loop.runReady();
  assert(started&&port>0);
  std::string authorizationCode;
  auto hold=std::make_shared<event_loop::Loop::KeepAlive>(loop.hold());
  listener.waitForAuthorization("expected-state",[&]{
    event_loop::Deferred<event_loop::Unit> ready(loop);
    ready.resolve({});return ready;
  }).then([&](const std::string* code,
      const std::exception_ptr& error){
    if(error)std::rethrow_exception(error);
    authorizationCode=*code;
    assert(listener.hasPendingResponse());
    listener.handleSuccessRedirect({"user:inference"});
    hold->release();
    loop.stop();
  });
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
  listener.close();
  assert(authorizationCode=="native-code");
  assert(response.starts_with("HTTP/1.1 302 Found\r\n"));
  assert(response.find("Location: http://127.0.0.1/success\r\n")!=
    std::string::npos);
  std::cout<<"native OAuth browser callback passed\n";
}
