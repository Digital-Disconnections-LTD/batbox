#include "cli/remote/IndigenousCcrHttp.hpp"
#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <vector>
#include <unistd.h>

using namespace batbox;
using namespace batbox::cli::remote;
namespace {
std::string readRequest(int fd){
  std::string bytes;char buffer[4096];
  while(bytes.find("\r\n\r\n")==std::string::npos){
    const auto count=recv(fd,buffer,sizeof(buffer),0);
    if(count<=0)throw std::runtime_error("request header missing");
    bytes.append(buffer,static_cast<std::size_t>(count));
  }
  const auto head=bytes.find("\r\n\r\n")+4;
  const auto lengthAt=bytes.find("Content-Length:");
  if(lengthAt!=std::string::npos){
    const auto end=bytes.find("\r\n",lengthAt);
    const auto length=std::stoul(bytes.substr(lengthAt+15,
      end-lengthAt-15));
    while(bytes.size()<head+length){
      const auto count=recv(fd,buffer,sizeof(buffer),0);
      if(count<=0)throw std::runtime_error("request body missing");
      bytes.append(buffer,static_cast<std::size_t>(count));
    }
  }
  return bytes;
}
void sendAll(int fd,const std::string& data){
  std::size_t offset=0;
  while(offset<data.size()){
    const auto count=send(fd,data.data()+offset,data.size()-offset,0);
    if(count<=0)throw std::runtime_error("response failed");
    offset+=static_cast<std::size_t>(count);
  }
}
template<class T>T await(event_loop::Loop& loop,
    event_loop::Deferred<T> pending){
  std::optional<T> value;std::exception_ptr error;
  pending.then([&](const T* data,const std::exception_ptr& failure){
    if(failure)error=failure;else if(data)value=*data;
  });
  const auto deadline=std::chrono::steady_clock::now()+
    std::chrono::seconds(5);
  while(!value&&!error&&std::chrono::steady_clock::now()<deadline){
    loop.runReady();std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  if(error)std::rethrow_exception(error);
  if(!value)throw std::runtime_error("native CCR HTTP timeout");
  return *value;
}
}
int main(){
  const int listener=socket(AF_INET,SOCK_STREAM,0);
  if(listener<0)throw std::runtime_error("socket failed");
  const int one=1;setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&one,sizeof(one));
  sockaddr_in address{};address.sin_family=AF_INET;
  address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);address.sin_port=0;
  if(bind(listener,reinterpret_cast<sockaddr*>(&address),sizeof(address))!=0||
     listen(listener,3)!=0)throw std::runtime_error("listen failed");
  socklen_t length=sizeof(address);
  getsockname(listener,reinterpret_cast<sockaddr*>(&address),&length);
  std::vector<std::string> requests;
  std::exception_ptr serverError;
  std::thread server([&]{
    try{
      for(int index=0;index<3;++index){
        int fd=accept(listener,nullptr,nullptr);
        if(fd<0)throw std::runtime_error("accept failed");
        requests.push_back(readRequest(fd));
        const auto body=index==0?"{\"worker\":{\"worker_epoch\":7}}":
          index==1?"{}":"{\"accepted\":true}";
        const auto status=index==2?"HTTP/1.1 429 Too Many Requests":
          "HTTP/1.1 200 OK";
        sendAll(fd,std::string(status)+"\r\nContent-Type: application/json\r\n"
          "Retry-After: 2\r\nContent-Length: "+std::to_string(
            std::strlen(body))+"\r\nConnection: close\r\n\r\n"+body);
        close(fd);
      }
    }catch(...){serverError=std::current_exception();}
  });
  event_loop::Loop loop;
  transport::ClientConfig config;
  config.connectTimeout=std::chrono::seconds(2);
  auto http=makeIndigenousCcrHttp(loop,config);
  const std::string base="http://127.0.0.1:"+
    std::to_string(ntohs(address.sin_port));
  const Json headers={{"Authorization","Bearer local"},
    {"Content-Type","application/json"}};
  const auto get=await(loop,http("get",base+"/worker",nullptr,headers,2000));
  const auto put=await(loop,http("put",base+"/worker",
    Json{{"worker_epoch",7}},headers,2000));
  const auto post=await(loop,http("post",base+"/events",
    Json{{"events",Json::array({1,2})}},headers,2000));
  shutdown(listener,SHUT_RDWR);close(listener);server.join();
  if(serverError)std::rethrow_exception(serverError);
  if(get.status!=200||get.data["worker"]["worker_epoch"]!=7||
     put.status!=200||post.status!=429||
     post.headers["retry-after"]!="2"||requests.size()!=3||
     requests[0].find("GET /worker HTTP/1.1")==std::string::npos||
     requests[1].find("PUT /worker HTTP/1.1")==std::string::npos||
     requests[1].find("\"worker_epoch\":7")==std::string::npos||
     requests[2].find("POST /events HTTP/1.1")==std::string::npos||
     requests[2].find("\"events\":[1,2]")==std::string::npos)
    throw std::runtime_error("native CCR HTTP protocol mismatch");
  std::cout<<"native CCR GET, PUT and POST passed\n";
}
