#include "mcp/http/Http.hpp"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace batbox;
namespace {
void check(bool condition,const std::string& message){
  if(!condition)throw std::runtime_error(message);
}
std::string readRequest(int peer){
  std::string request;char buffer[4096];
  for(;;){
    const auto header=request.find("\r\n\r\n");
    if(header!=std::string::npos){
      const auto at=request.find("Content-Length: ");
      const auto length=at==std::string::npos||at>header?0:
        std::stoul(request.substr(at+16,request.find("\r\n",at)-at-16));
      if(request.size()>=header+4+length)return request;
    }
    const auto count=::read(peer,buffer,sizeof(buffer));
    if(count<=0)return request;
    request.append(buffer,static_cast<std::size_t>(count));
  }
}
}
int main(){
  try{
    const int listener=::socket(AF_INET,SOCK_STREAM,0);check(listener>=0,"socket");
    sockaddr_in address{};address.sin_family=AF_INET;
    address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);address.sin_port=0;
    check(::bind(listener,reinterpret_cast<sockaddr*>(&address),sizeof(address))==0,"bind");
    check(::listen(listener,4)==0,"listen");
    socklen_t size=sizeof(address);
    check(::getsockname(listener,reinterpret_cast<sockaddr*>(&address),&size)==0,"name");
    const auto port=std::to_string(ntohs(address.sin_port));
    std::vector<std::string> seen;
    std::thread server([&]{
      // Same-origin redirect, its target, then a redirect to another host.
      const std::vector<std::string> replies{
        "HTTP/1.1 307 Temporary Redirect\r\nLocation: /mcp/\r\nContent-Length: 5\r\n"
          "Connection: close\r\n\r\nmoved",
        "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 11\r\n"
          "Connection: close\r\n\r\n{\"ok\":true}",
        "HTTP/1.1 307 Temporary Redirect\r\nLocation: http://localhost:"+port+
          "/elsewhere\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"};
      for(const auto& reply:replies){
        const int peer=::accept(listener,nullptr,nullptr);
        if(peer<0)return;
        seen.push_back(readRequest(peer));
        (void)::write(peer,reply.data(),reply.size());
        ::close(peer);
      }
    });
    mcp::http::Request request;
    request.method="POST";request.url="http://127.0.0.1:"+port+"/mcp";
    request.body="{\"jsonrpc\":\"2.0\"}";
    request.headers={{"content-type","application/json"},{"x-api-key","secret"}};
    std::string streamed;
    request.onData=[&](std::string_view bytes){streamed.append(bytes);return true;};
    const auto followed=mcp::http::fetch(request);
    check(followed.status==200,"a same-origin redirect must be followed");
    check(streamed=="{\"ok\":true}","the redirect's own body reached the caller: "+streamed);
    request.url="http://127.0.0.1:"+port+"/cross";streamed.clear();
    bool refused=false;
    try{(void)mcp::http::fetch(request);}
    catch(const std::exception& error){
      refused=std::string(error.what()).find("on another host")!=std::string::npos;
    }
    ::shutdown(listener,SHUT_RDWR);::close(listener);
    server.join();
    check(refused,"a redirect to another host must be refused");
    check(seen.size()==3,"the other host was contacted or a request is missing");
    check(seen[1].starts_with("POST /mcp/ HTTP/1.1")&&
      seen[1].ends_with("{\"jsonrpc\":\"2.0\"}"),"a 307 must repeat the POST with its body");
    std::cout<<"MCP requests with credentials follow redirects only within their origin\n";
    return 0;
  }catch(const std::exception& error){
    std::cerr<<error.what()<<"\n";
    return 1;
  }
}
