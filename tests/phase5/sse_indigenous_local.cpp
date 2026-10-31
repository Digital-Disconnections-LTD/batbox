#include "cli/remote/IndigenousSSE.hpp"
#include "runtime/Url.hpp"
#include <arpa/inet.h>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

using namespace batbox;
using namespace batbox::cli::remote;
namespace {
std::string receiveRequest(int fd){
  std::string data;
  char bytes[4096];
  while(data.find("\r\n\r\n")==std::string::npos){
    const auto received=recv(fd,bytes,sizeof(bytes),0);
    if(received<=0)throw std::runtime_error("request closed early");
    data.append(bytes,static_cast<std::size_t>(received));
  }
  const auto headerEnd=data.find("\r\n\r\n")+4;
  const auto lengthAt=data.find("Content-Length:");
  if(lengthAt!=std::string::npos){
    const auto lineEnd=data.find("\r\n",lengthAt);
    const auto length=std::stoul(data.substr(lengthAt+15,
      lineEnd-(lengthAt+15)));
    while(data.size()<headerEnd+length){
      const auto received=recv(fd,bytes,sizeof(bytes),0);
      if(received<=0)throw std::runtime_error("body closed early");
      data.append(bytes,static_cast<std::size_t>(received));
    }
  }
  return data;
}
void sendAll(int fd,const std::string& data){
  std::size_t sent=0;
  while(sent<data.size()){
    const auto count=send(fd,data.data()+sent,data.size()-sent,0);
    if(count<=0)throw std::runtime_error("send failed");
    sent+=static_cast<std::size_t>(count);
  }
}
}
int main(){
  int listener=socket(AF_INET,SOCK_STREAM,0);
  if(listener<0)throw std::runtime_error("socket failed");
  const int one=1;
  setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&one,sizeof(one));
  sockaddr_in address{};address.sin_family=AF_INET;
  address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);address.sin_port=0;
  if(bind(listener,reinterpret_cast<sockaddr*>(&address),sizeof(address))!=0||
     listen(listener,2)!=0)throw std::runtime_error("listen failed");
  socklen_t addressLength=sizeof(address);
  getsockname(listener,reinterpret_cast<sockaddr*>(&address),&addressLength);
  std::string getRequest,postRequest;
  std::exception_ptr serverError;
  std::thread server([&]{
    try{
      for(int count=0;count<2;++count){
        int fd=accept(listener,nullptr,nullptr);
        if(fd<0)throw std::runtime_error("accept failed");
        const auto request=receiveRequest(fd);
        if(request.rfind("GET ",0)==0){
          getRequest=request;
          const std::string frame="id: 9\nevent: client_event\ndata: "
            "{\"event_id\":\"event-9\",\"sequence_num\":9,"
            "\"event_type\":\"message\",\"source\":\"worker\","
            "\"payload\":{\"type\":\"user\",\"text\":\"local\"},"
            "\"created_at\":\"now\"}\n\n";
          sendAll(fd,"HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\n"
            "Content-Length: "+std::to_string(frame.size())+
            "\r\nConnection: close\r\n\r\n"+frame);
        }else{
          postRequest=request;
          sendAll(fd,"HTTP/1.1 201 Created\r\nContent-Length: 2\r\n"
            "Connection: close\r\n\r\n{}");
        }
        close(fd);
      }
    }catch(...){serverError=std::current_exception();}
  });
  event_loop::Loop loop;
  IndigenousSseConfig config;
  config.network.connectTimeout=std::chrono::seconds(2);
  config.network.requestTimeout=std::chrono::seconds(2);
  config.getAuthHeaders=[]{return Json{{"Authorization","Bearer local"}};};
  config.userBat=[]{return "batbox-native-test";};
  config.logDebug=[](const std::string&,const Json&){};
  config.protocolVersionHeaderName="batbox-version";
  config.protocolVersionHeaderValue="2026-10-29";
  const auto url=runtime::parseUrl("http://127.0.0.1:"+
    std::to_string(ntohs(address.sin_port))+"/v2/events/stream?x=1");
  if(!url)throw std::runtime_error("URL parse failed");
  auto transport=std::make_shared<SSETransport>(loop,*url,Json::object(),
    "local-session",std::function<Json()>{},7,
    makeIndigenousSseServices(loop,std::move(config)));
  std::string received;
  transport->setOnData([&](const std::string& value){received+=value;});
  bool connectedDone=false,postDone=false;
  transport->connect().then([&](const auto*,const auto& error){
    if(error)std::rethrow_exception(error);
    connectedDone=true;
  });
  transport->write(Json{{"type","assistant"},{"text","sent"}})
    .then([&](const auto*,const auto& error){
      if(error)std::rethrow_exception(error);
      postDone=true;
    });
  const auto deadline=std::chrono::steady_clock::now()+
    std::chrono::seconds(5);
  while((!connectedDone||!postDone)&&
        std::chrono::steady_clock::now()<deadline){
    loop.runReady();std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  transport->close();
  close(listener);server.join();
  if(serverError)std::rethrow_exception(serverError);
  if(!connectedDone||!postDone||transport->getLastSequenceNum()!=9||
     received!="{\"type\":\"user\",\"text\":\"local\"}\n"||
     getRequest.find("from_sequence_num=7")==std::string::npos||
     getRequest.find("Last-Event-ID: 7")==std::string::npos||
     postRequest.find("POST /v2/events HTTP/1.1")==std::string::npos||
     postRequest.find("\"text\":\"sent\"")==std::string::npos)
    throw std::runtime_error("native SSE local protocol mismatch");
  std::cout<<"native SSE stream and POST passed\n";
}
