#include "auth/oauth_client/IndigenousAccountHttp.hpp"
#include "runtime/JsonValue.hpp"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

int main(){
  using namespace batbox;
  const int listener=::socket(AF_INET,SOCK_STREAM,0);
  assert(listener>=0);
  sockaddr_in address{};
  address.sin_family=AF_INET;
  address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
  address.sin_port=0;
  assert(::bind(listener,reinterpret_cast<sockaddr*>(&address),
    sizeof(address))==0);
  assert(::listen(listener,1)==0);
  socklen_t size=sizeof(address);
  assert(::getsockname(listener,reinterpret_cast<sockaddr*>(&address),
    &size)==0);
  const auto port=ntohs(address.sin_port);
  std::string captured;
  std::thread server([&]{
    const int client=::accept(listener,nullptr,nullptr);
    if(client<0)throw std::runtime_error("Accept failed");
    char bytes[4096];
    for(;;){
      const auto count=::recv(client,bytes,sizeof(bytes),0);
      if(count<=0)throw std::runtime_error("Read failed");
      captured.append(bytes,static_cast<std::size_t>(count));
      if(captured.find("\r\n\r\n")!=std::string::npos)break;
    }
    const std::string body="{\"raw_key\":\"native-key\"}";
    const std::string reply="HTTP/1.1 200 OK\r\nContent-Type: "
      "application/json\r\nContent-Length: "+std::to_string(body.size())+
      "\r\nConnection: close\r\n\r\n"+body;
    if(::send(client,reply.data(),reply.size(),0)<0)
      throw std::runtime_error("Send failed");
    ::close(client);
  });
  event_loop::Loop loop;
  auto response=auth::oauth_client::indigenousEmptyPost(loop,
    "http://127.0.0.1:"+std::to_string(port)+"/key",
    Json{{"Authorization","Bearer native-token"}});
  bool done=false;
  Json value;
  std::exception_ptr error;
  response.then([&](const auth::refresh::HttpResponse* result,
      const std::exception_ptr& caught){
    if(caught)error=caught;else value=result->data;
    done=true;
  });
  for(int i=0;i<200&&!done;++i)loop.runReady();
  if(!done)loop.run();
  server.join();
  ::close(listener);
  if(error)std::rethrow_exception(error);
  assert(value["raw_key"]=="native-key");
  assert(captured.starts_with("POST /key HTTP/1.1\r\n"));
  assert(captured.find("Authorization: Bearer native-token")!=
    std::string::npos);
  assert(captured.find("Content-Length: 0")!=std::string::npos);
  std::cout<<"native OAuth empty-body POST passed\n";
}
