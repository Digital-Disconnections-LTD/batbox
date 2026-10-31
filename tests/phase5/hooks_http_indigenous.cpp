#include "instructions/hooks/runtime/IndigenousHttp.hpp"
#include <arpa/inet.h>
#include <cassert>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string>
#include <thread>

using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
int main(){
  int listener=::socket(AF_INET,SOCK_STREAM,0);assert(listener>=0);
  sockaddr_in address{};address.sin_family=AF_INET;
  address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);address.sin_port=0;
  assert(::bind(listener,reinterpret_cast<sockaddr*>(&address),sizeof(address))==0);
  assert(::listen(listener,1)==0);
  socklen_t length=sizeof(address);
  assert(::getsockname(listener,reinterpret_cast<sockaddr*>(&address),&length)==0);
  const auto port=ntohs(address.sin_port);
  std::string observed;
  std::thread server([&]{
    const int peer=::accept(listener,nullptr,nullptr);assert(peer>=0);
    char buffer[4096];
    for(;;){
      const auto count=::read(peer,buffer,sizeof(buffer));assert(count>0);
      observed.append(buffer,static_cast<std::size_t>(count));
      if(observed.find("\r\n\r\n")!=std::string::npos&&
        observed.ends_with("{\"x\":1}"))break;
    }
    const std::string reply="HTTP/1.1 201 Created\r\nContent-Length: 6\r\n"
      "Connection: close\r\n\r\nhello\n";
    assert(::write(peer,reply.data(),reply.size())==static_cast<ssize_t>(reply.size()));
    ::close(peer);::close(listener);
  });
  event_loop::Loop loop;
  hook::HttpTransportRequest request;
  request.url="http://127.0.0.1:"+std::to_string(port)+"/hook";
  request.body="{\"x\":1}";
  request.headers={{"Content-Type","application/json"}};
  request.useSsrfGuard=true;
  bool completed=false;
  hook::indigenousHttpPost(loop)(request).then(
    [&](const hook::HttpTransportResponse* response,const std::exception_ptr& error){
      assert(!error);assert(response);
      assert(response->status==201);assert(response->body=="hello\n");
      completed=true;
    });
  loop.run();server.join();
  assert(completed);
  assert(observed.starts_with("POST /hook HTTP/1.1\r\n"));
  assert(observed.find("Content-Type: application/json")!=std::string::npos);
  hook::HttpTransportRequest blocked;
  blocked.url="http://169.254.169.254/latest/meta-data";
  blocked.body="{}";blocked.useSsrfGuard=true;
  bool rejected=false;
  hook::indigenousHttpPost(loop)(blocked).then(
    [&](const hook::HttpTransportResponse*,const std::exception_ptr& error){
      assert(error);
      try{std::rethrow_exception(error);}catch(const std::exception& value){
        rejected=std::string(value.what()).starts_with("HTTP hook blocked:");
      }
    });
  loop.run();assert(rejected);
}
