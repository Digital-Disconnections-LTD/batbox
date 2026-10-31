#include "settings/remote/Http.hpp"
#include "settings/remote/IndigenousFile.hpp"
#include "runtime/JsonValue.hpp"
#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace {
void require(bool condition,const char* message){
  if(!condition)throw std::runtime_error(message);
}
void wait(batbox::event_loop::Loop& loop,
    batbox::event_loop::Deferred<batbox::event_loop::Unit> promise){
  bool done=false;
  promise.then([&](const batbox::event_loop::Unit*,
      const std::exception_ptr& error){
    if(error)std::rethrow_exception(error);
    done=true;
  });
  loop.run();require(done,"file job did not finish");
}
}
int main(){
  using namespace batbox;
  using namespace batbox::settings::remote;
  event_loop::Loop loop;
  const int server=::socket(AF_INET,SOCK_STREAM,0);
  if(server<0)throw std::runtime_error("socket failed");
  sockaddr_in address{};address.sin_family=AF_INET;
  address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);address.sin_port=0;
  if(::bind(server,reinterpret_cast<sockaddr*>(&address),sizeof(address))!=0||
      ::listen(server,4)!=0)throw std::runtime_error("listen failed");
  socklen_t length=sizeof(address);
  if(::getsockname(server,reinterpret_cast<sockaddr*>(&address),&length)!=0)
    throw std::runtime_error("getsockname failed");
  const auto port=ntohs(address.sin_port);
  std::string seen;
  std::thread worker([&](){
    const char* responses[]={
      "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
        "Content-Length: 32\r\nConnection: close\r\n\r\n"
        "{\"uuid\":\"id\",\"settings\":{\"x\":1}}",
      "HTTP/1.1 304 Not Modified\r\nContent-Length: 0\r\n"
        "Connection: close\r\n\r\n",
      "HTTP/1.1 401 Unauthorized\r\nContent-Length: 0\r\n"
        "Connection: close\r\n\r\n"};
    for(const char* response:responses){
      const int client=::accept(server,nullptr,nullptr);
      if(client<0)break;
      std::string request;
      char buffer[4096];
      while(request.find("\r\n\r\n")==std::string::npos){
        const auto count=::recv(client,buffer,sizeof(buffer),0);
        if(count<=0)break;
        request.append(buffer,static_cast<std::size_t>(count));
      }
      seen+=request;
      const auto size=std::strlen(response);
      std::size_t sent=0;
      while(sent<size){
        const auto count=::send(client,response+sent,size-sent,0);
        if(count<=0)break;
        sent+=static_cast<std::size_t>(count);
      }
      ::close(client);
    }
  });
  try{
    const auto url="http://127.0.0.1:"+std::to_string(port)+"/settings";
    const auto get=[&](const Json& headers){
      std::optional<HttpResponse> value;
      std::exception_ptr error;
      indigenousHttpGet(loop,url,headers,1000).then([&](
          const HttpResponse* response,const std::exception_ptr& failure){
        if(failure)error=failure;else value=*response;
      });
      loop.run();
      if(error)std::rethrow_exception(error);
      require(value.has_value(),"HTTP request did not finish");
      return *value;
    };
    const auto first=get(Json{{"If-None-Match","\"sha256:test\""}});
    require(first.status==200&&first.data["settings"]["x"]==1,
      "HTTP 200 JSON body mismatch");
    require(get(Json::object()).status==304,"HTTP 304 mismatch");
    bool unauthorized=false;
    try{(void)get(Json::object());}
    catch(const HttpError& error){unauthorized=error.kind=="auth"&&
      error.status==401;}
    require(unauthorized,"HTTP 401 classification mismatch");
    require(seen.find("If-None-Match: \"sha256:test\"")!=
      std::string::npos,"ETag header missing");
    worker.join();::close(server);
    const auto stamp=std::chrono::steady_clock::now()
      .time_since_epoch().count();
    const auto file=std::filesystem::temp_directory_path()/
      ("batbox-remote-"+std::to_string(::getpid())+"-"+
        std::to_string(stamp)+".json");
    const Json settings={{"a",1},{"nested",Json{{"b",2}}}};
    wait(loop,saveSettings(loop,settings,[&](){return file.string();},
      [](const std::string&){}));
    std::ifstream input(file,std::ios::binary);
    const std::string contents((std::istreambuf_iterator<char>(input)),{});
    require(contents==value::stringify(settings,2),"saved JSON differs");
    const auto permissions=std::filesystem::status(file).permissions();
    require((permissions&std::filesystem::perms::group_all)==
      std::filesystem::perms::none,"saved file group permissions");
    wait(loop,unlinkSettings(loop,file.string()));
    require(!std::filesystem::exists(file),"remote cache file not removed");
    std::cout<<"Native remote HTTP/file fixture passed\n";
  }catch(...){::shutdown(server,SHUT_RDWR);::close(server);worker.join();throw;}
}
