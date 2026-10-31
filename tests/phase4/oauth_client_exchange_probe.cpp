#include "auth/oauth_client/TokenExchange.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>

int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    event_loop::Loop loop;
    Json trace=Json::array(),value=nullptr,error=nullptr;
    auth::oauth_client::ExchangeServices services;
    services.getConfig=[&]{
      trace.push_back("config");
      client_factory::oauth_config::Config result;
      result.manualRedirectUrl=test.value("manualUrl",
        std::string("https://oauth.example/callback"));
      result.clientId=test.value("clientId",std::string("client-1"));
      result.tokenUrl=test.value("tokenUrl",
        std::string("https://oauth.example/token"));
      return result;
    };
    services.httpPost=[&](const std::string& url,const Json& body,
        const Json& headers,double timeout){
      trace.push_back(Json::array({"post",url,body,
        Json{{"headers",headers},{"timeout",timeout}}}));
      event_loop::Deferred<auth::refresh::HttpResponse> response(loop);
      if(test.value("throwHttp",false))response.reject(
        std::make_exception_ptr(std::runtime_error("network failure")));
      else response.resolve({test.value("status",200),
        test.value("statusText",std::string("OK")),
        test.value("data",Json{{"access_token","token-1"}})});
      return response;
    };
    auth::oauth_client::ExchangeArgs args;
    args.authorizationCode=test.value("code",std::string("auth-code"));
    args.state=test.value("state",std::string("state-1"));
    args.codeVerifier=test.value("verifier",std::string("verifier-1"));
    args.port=test.value("port",3456);
    args.useManualRedirect=test.value("manual",false);
    if(test.contains("expiresIn")&&!test["expiresIn"].is_null())
      args.expiresIn=test["expiresIn"].get<double>();
    bool done=false;
    auth::oauth_client::exchangeCodeForTokens(loop,args,services).then(
      [&](const Json* result,const std::exception_ptr& caught){
        if(caught){try{std::rethrow_exception(caught);}
          catch(const std::exception& cause){error=cause.what();}}
        else value=*result;
        done=true;
      });
    for(int i=0;i<100&&!done;++i)loop.runReady();
    if(!done)throw std::runtime_error("OAuth exchange did not settle");
    std::cout<<value::stringify(Json{{"value",value},{"error",error},
      {"trace",trace}})<<'\n';
  }
}
