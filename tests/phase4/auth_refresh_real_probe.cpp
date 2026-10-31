#include "auth/refresh/IndigenousHttp.hpp"
#include "auth/refresh/ProfileHttp.hpp"
#include "auth/refresh/Refresh.hpp"
#include <arpa/inet.h>
#include <cassert>
#include <cstring>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {
std::size_t contentLength(const std::string& request){
  const auto key=request.find("Content-Length:");
  if(key==std::string::npos)return 0;
  const auto start=key+std::strlen("Content-Length:");
  return static_cast<std::size_t>(std::stoul(request.substr(start)));
}
std::string receiveRequest(int fd){
  std::string request;
  char bytes[4096];
  while(true){
    const auto count=::read(fd,bytes,sizeof(bytes));
    if(count<=0)throw std::runtime_error("Short local HTTP request");
    request.append(bytes,static_cast<std::size_t>(count));
    const auto end=request.find("\r\n\r\n");
    if(end!=std::string::npos&&request.size()>=
        end+4+contentLength(request))return request;
  }
}
void sendResponse(int fd,const std::string& body){
  const auto response="HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
    "Content-Length: "+std::to_string(body.size())+
    "\r\nConnection: close\r\n\r\n"+body;
  std::size_t sent=0;
  while(sent<response.size()){
    const auto count=::write(fd,response.data()+sent,response.size()-sent);
    if(count<=0)throw std::runtime_error("Short local HTTP response");
    sent+=static_cast<std::size_t>(count);
  }
}
}
int main(){
  using namespace batbox;
  const int server=::socket(AF_INET,SOCK_STREAM,0);
  assert(server>=0);
  sockaddr_in address{};
  address.sin_family=AF_INET;
  address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
  address.sin_port=0;
  assert(::bind(server,reinterpret_cast<sockaddr*>(&address),
    sizeof(address))==0);
  assert(::listen(server,2)==0);
  socklen_t size=sizeof(address);
  assert(::getsockname(server,reinterpret_cast<sockaddr*>(&address),
    &size)==0);
  const auto base="http://127.0.0.1:"+
    std::to_string(ntohs(address.sin_port));
  std::vector<std::string> requests;
  std::string serverError;
  std::thread listener([&]{
    try{
      for(int index=0;index<2;++index){
        pollfd wait{server,POLLIN,0};
        if(::poll(&wait,1,5000)<=0)
          throw std::runtime_error("Local HTTP request timed out");
        const int client=::accept(server,nullptr,nullptr);
        if(client<0)throw std::runtime_error("Local HTTP accept failed");
        const auto request=receiveRequest(client);
        requests.push_back(request);
        if(request.starts_with("POST /token "))
          sendResponse(client,"{\"access_token\":\"new-access\","
            "\"refresh_token\":\"next-refresh\","
            "\"expires_in\":60,\"scope\":\"user:inference\"}");
        else if(request.starts_with("GET /api/oauth/profile "))
          sendResponse(client,"{\"organization\":{"
            "\"organization_type\":\"batbox_team\","
            "\"rate_limit_tier\":\"tier\","
            "\"billing_type\":\"monthly\","
            "\"has_extra_usage_enabled\":true},"
            "\"account\":{\"display_name\":\"Ada\"}}");
        else throw std::runtime_error("Unexpected local HTTP path");
        ::close(client);
      }
    }catch(const std::exception& error){serverError=error.what();}
    ::close(server);
  });
  event_loop::Loop loop;
  Json config={{"oauthAccount",Json::object()}};
  auth::refresh::ProfileHttpServices profileHttp;
  profileHttp.baseApiUrl=[&](){return base;};
  profileHttp.httpGet=[&](const std::string& url,const Json& headers,
      double timeout){return auth::refresh::indigenousHttpGet(loop,url,headers,timeout);};
  profileHttp.logError=[](const std::string& error){
    throw std::runtime_error("Unexpected profile HTTP error: "+error);};
  auth::refresh::ProfileServices profile;
  profile.subscriptionTypes={{"batbox_team","team"}};
  profile.getRawProfile=[&](const std::string& token){
    return auth::refresh::getRawOAuthProfile(loop,token,profileHttp);};
  auth::refresh::Services services;
  services.tokenUrl=[&](){return base+"/token";};
  services.clientId=[](){return std::string("batbox-client");};
  services.defaultScopes={"user:inference"};
  services.httpPost=[&](const std::string& url,const Json& body,
      const Json& headers,double timeout){
    return auth::refresh::indigenousHttpPost(loop,url,body,headers,timeout);};
  services.nowMs=[](){return 1000.0;};
  services.globalConfig=[&](){return config;};
  services.existingTokens=[]()->std::optional<Json>{return std::nullopt;};
  services.fetchProfileInfo=[&](const std::string& token){
    return auth::refresh::fetchProfileInfo(loop,token,profile);};
  services.saveGlobalConfig=[&](const std::function<Json(const Json&)>& change){
    config=change(config);};
  auto promise=auth::refresh::refreshOAuthToken(loop,"old-refresh",
    std::nullopt,services);
  Json result;
  bool settled=false;
  promise.then([&](const Json* value,const std::exception_ptr& error){
    if(error)std::rethrow_exception(error);
    result=*value;settled=true;
  });
  loop.run();listener.join();
  assert(serverError.empty()&&settled&&requests.size()==2);
  assert(requests[0].find("POST /token ")==0);
  assert(requests[0].find("\"refresh_token\":\"old-refresh\"")!=
    std::string::npos);
  assert(requests[1].find("GET /api/oauth/profile ")==0);
  assert(requests[1].find("Authorization: Bearer new-access")!=
    std::string::npos);
  assert(result["accessToken"]=="new-access"&&
    result["subscriptionType"]=="team"&&
    result["rateLimitTier"]=="tier");
  assert(config["oauthAccount"]["displayName"]=="Ada");
  std::cout<<"Native OAuth refresh/profile HTTP fixture passed\n";
}
