#include "auth/oauth_client/IndigenousBinding.hpp"
#include "runtime/JsonValue.hpp"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cassert>
#include <cerrno>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
struct Socket {
  int fd=-1;
  explicit Socket(int value):fd(value){}
  ~Socket(){if(fd>=0)::close(fd);}
};
std::size_t contentLength(const std::string& request){
  const auto start=request.find("Content-Length:");
  if(start==std::string::npos)throw std::runtime_error("Missing length");
  return std::stoul(request.substr(start+15));
}
}
int main(){
  using namespace batbox;
  Socket listener(::socket(AF_INET,SOCK_STREAM,0));
  assert(listener.fd>=0);
  sockaddr_in address{};
  address.sin_family=AF_INET;
  address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
  address.sin_port=0;
  assert(::bind(listener.fd,reinterpret_cast<sockaddr*>(&address),
    sizeof(address))==0);
  assert(::listen(listener.fd,1)==0);
  socklen_t size=sizeof(address);
  assert(::getsockname(listener.fd,reinterpret_cast<sockaddr*>(&address),
    &size)==0);
  const auto port=ntohs(address.sin_port);
  std::string captured;
  std::thread server([&]{
    Socket client(::accept(listener.fd,nullptr,nullptr));
    if(client.fd<0)throw std::runtime_error("Accept failed");
    char buffer[4096];
    for(;;){
      const auto count=::recv(client.fd,buffer,sizeof(buffer),0);
      if(count<=0)throw std::runtime_error("Read failed");
      captured.append(buffer,static_cast<std::size_t>(count));
      const auto head=captured.find("\r\n\r\n");
      if(head!=std::string::npos&&
          captured.size()>=head+4+contentLength(captured))break;
    }
    const std::string body="{\"access_token\":\"native-token\"}";
    const std::string response="HTTP/1.1 200 OK\r\nContent-Type: "
      "application/json\r\nContent-Length: "+std::to_string(body.size())+
      "\r\nConnection: close\r\n\r\n"+body;
    if(::send(client.fd,response.data(),response.size(),0)<0)
      throw std::runtime_error("Send failed");
  });
  event_loop::Loop loop;
  client_factory::oauth_config::Services config;
  config.data.production.tokenUrl="http://127.0.0.1:"+
    std::to_string(port)+"/token";
  config.data.production.clientId="native-client";
  config.environment=[](const std::string&)->Json{return nullptr;};
  auto services=auth::oauth_client::makeIndigenousExchangeServices(loop,config);
  auth::oauth_client::ExchangeArgs args;
  args.authorizationCode="native-code";
  args.state="native-state";
  args.codeVerifier="native-verifier";
  args.port=8080;
  bool done=false;
  Json value;
  std::exception_ptr error;
  auth::oauth_client::exchangeCodeForTokens(loop,args,services).then(
    [&](const Json* result,const std::exception_ptr& caught){
      if(caught)error=caught;else value=*result;
      done=true;
    });
  for(int i=0;i<200&&!done;++i)loop.runReady();
  if(!done)loop.run();
  server.join();
  if(error)std::rethrow_exception(error);
  assert(value["access_token"]=="native-token");
  assert(captured.starts_with("POST /token HTTP/1.1\r\n"));
  const auto head=captured.find("\r\n\r\n");
  assert(head!=std::string::npos);
  const auto body=value::parse(captured.substr(head+4));
  assert(body["grant_type"]=="authorization_code");
  assert(body["client_id"]=="native-client");
  assert(body["redirect_uri"]=="http://localhost:8080/callback");
  std::cout<<"native OAuth code exchange passed\n";
}
