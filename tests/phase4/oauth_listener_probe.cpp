#include "auth/oauth_listener/AuthCodeListener.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>

namespace {
using namespace batbox;
struct FakeResponse:auth::oauth_listener::Response {
  Json& trace;
  Json status=nullptr,headers=Json::object(),body=nullptr;
  explicit FakeResponse(Json& value):trace(value){}
  void writeHead(int code,const Json& values)override{
    trace.push_back(Json::array({"writeHead",code,values}));
    status=code;headers=values;
  }
  void end(const std::string& text)override{
    trace.push_back(Json::array({"end",text}));body=text;
  }
  Json result()const{return Json{{"status",status},{"headers",headers},
    {"body",body}};}
};
struct FakeServer:auth::oauth_listener::Server {
  event_loop::Loop& loop;
  Json& trace;
  std::function<void(const auth::oauth_listener::Request&)> request;
  std::function<void(const std::string&)> error;
  explicit FakeServer(event_loop::Loop& owner,Json& value):
    loop(owner),trace(value){}
  event_loop::Deferred<int> start(const std::optional<int>& port)override{
    trace.push_back(Json::array({"once","error"}));
    trace.push_back(Json::array({"listen",port.value_or(0),"localhost"}));
    event_loop::Deferred<int> result(loop);
    result.resolve(port.value_or(0)?*port:49152);return result;
  }
  void onRequest(std::function<void(const auth::oauth_listener::Request&)>
      handler)override{trace.push_back(Json::array({"on","request"}));
    request=std::move(handler);}
  void onError(std::function<void(const std::string&)> handler)override{
    trace.push_back(Json::array({"on","error"}));
    error=std::move(handler);}
  void removeAllListeners()override{
    trace.push_back("removeAllListeners");request={};error={};}
  void close()override{trace.push_back("close");}
};
}
int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    event_loop::Loop loop;
    Json trace=Json::array(),value=nullptr,error=nullptr;
    auto server=std::make_shared<FakeServer>(loop,trace);
    auth::oauth_listener::Services services;
    services.createServer=[&]{trace.push_back("createServer");
      return server;};
    services.getConfig=[&]{trace.push_back("config");
      client_factory::oauth_config::Config result;
      result.appSuccessUrl="https://example.test/app";
      result.consoleSuccessUrl="https://example.test/console";
      return result;};
    services.logError=[&](const std::string& message){
      trace.push_back(Json::array({"logError",message}));};
    services.inferenceScope="user:inference";
    auth::oauth_listener::AuthCodeListener listener(loop,services,
      test.value("callbackPath",std::string("/callback")));
    std::optional<int> requestedPort;
    if(test.contains("port"))requestedPort=test["port"].get<int>();
    int port=0;
    bool started=false;
    listener.start(requestedPort).then([&](const int* number,
        const std::exception_ptr& caught){
      if(caught)std::rethrow_exception(caught);
      port=*number;started=true;});
    for(int i=0;i<20&&!started;++i)loop.runReady();
    if(!started)throw std::runtime_error("OAuth listener start pending");
    bool settled=false;
    listener.waitForAuthorization("state-1",[&]{
      trace.push_back("ready");
      event_loop::Deferred<event_loop::Unit> ready(loop);
      ready.resolve({});return ready;
    }).then([&](const std::string* code,const std::exception_ptr& caught){
      if(caught){try{std::rethrow_exception(caught);}
        catch(const std::exception& cause){error=cause.what();}}
      else value=*code;
      settled=true;
    });
    std::vector<std::shared_ptr<FakeResponse>> responses;
    auto request=[&](const std::string& url){
      auto response=std::make_shared<FakeResponse>(trace);
      responses.push_back(response);
      trace.push_back(Json::array({"emit","request"}));
      server->request({url,"localhost",response});
    };
    if(test.value("first404",false))
      request("/wrong?code=ignored&state=state-1");
    request(test.value("url",std::string(
      "/callback?code=auth-code&state=state-1")));
    for(int i=0;i<20&&!settled;++i)loop.runReady();
    if(!settled)throw std::runtime_error("OAuth listener wait pending");
    const bool pendingBefore=listener.hasPendingResponse();
    const auto redirect=test.value("redirect",std::string());
    if(redirect=="error")listener.handleErrorRedirect();
    else if(redirect=="close")listener.close();
    else if(redirect=="custom")listener.handleSuccessRedirect({},
      [&](auth::oauth_listener::Response& response,
          const std::vector<std::string>& scopes){
        trace.push_back(Json::array({"custom",scopes}));
        response.writeHead(201,Json::object());response.end("custom");});
    else{
      std::vector<std::string> scopes;
      if(test.contains("scopes"))for(const auto& scope:test["scopes"])
        scopes.push_back(scope.get<std::string>());
      listener.handleSuccessRedirect(scopes);
    }
    Json row=Json::array();
    for(const auto& response:responses)row.push_back(response->result());
    const Json result={{"port",port},{"value",value},{"error",error},
      {"pendingBefore",pendingBefore},
      {"pendingAfter",listener.hasPendingResponse()},
      {"responses",row},{"trace",trace}};
    std::cout<<value::stringify(result)<<'\n';
  }
}
