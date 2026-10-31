#include "auth/refresh/Refresh.hpp"
#include <iostream>
#include <stdexcept>

int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    event_loop::Loop loop;
    Json trace=Json::array(),config=test.value("config",Json::object());
    auth::refresh::ProfileServices profiles;
    profiles.subscriptionTypes={{"claude_max","max"},{"claude_pro","pro"},
      {"claude_enterprise","enterprise"},{"claude_team","team"}};
    profiles.getRawProfile=[&](const std::string& token){
      trace.push_back(Json::array({"profile",token}));
      event_loop::Deferred<std::optional<Json>> output(loop);
      if(test.value("profileError",false))output.reject(
        std::make_exception_ptr(std::runtime_error("profile failed")));
      else if(test.contains("profile")&&!test["profile"].is_null())
        output.resolve(test["profile"]);
      else output.resolve(std::nullopt);
      return output;
    };
    auth::refresh::Services services;
    services.clientId=[&](){trace.push_back("clientId");
      return std::string("batbox-client");};
    services.tokenUrl=[&](){trace.push_back("tokenUrl");
      return std::string("https://auth.batbox.test/token");};
    services.defaultScopes={"user:inference","user:profile"};
    services.httpPost=[&](const std::string& url,const Json& body,
        const Json& headers,double timeout){
      trace.push_back(Json::array({"post",url,body,
        Json{{"headers",headers},{"timeout",timeout}}}));
      event_loop::Deferred<auth::refresh::HttpResponse> output(loop);
      if(test.value("postError",false))output.reject(
        std::make_exception_ptr(std::runtime_error("post failed")));
      else{
        const auto response=test.value("response",Json::object());
        auth::refresh::HttpResponse value;
        value.status=response.value("status",200);
        value.statusText=response.value("statusText",std::string("OK"));
        value.data=response.value("data",Json{{"access_token","access"},
          {"expires_in",3600},{"scope","user:inference"}});
        output.resolve(value);
      }
      return output;
    };
    services.nowMs=[&](){trace.push_back("now");
      return test.value("now",1000.0);};
    services.globalConfig=[&](){trace.push_back("config");return config;};
    services.existingTokens=[&]()->std::optional<Json>{
      trace.push_back("existing");
      if(test.contains("existing")&&!test["existing"].is_null())
        return test["existing"];
      return std::nullopt;
    };
    services.fetchProfileInfo=[&](const std::string& token){
      return auth::refresh::fetchProfileInfo(loop,token,profiles);};
    services.saveGlobalConfig=[&](const std::function<Json(const Json&)>& update){
      trace.push_back("save");config=update(config);
    };
    std::optional<std::vector<std::string>> scopes;
    if(test.contains("requestedScopes")&&!test["requestedScopes"].is_null())
      scopes=test["requestedScopes"].get<std::vector<std::string>>();
    event_loop::Deferred<Json> operation(loop);
    if(test.value("operation",std::string{})=="profile")
      operation=auth::refresh::fetchProfileInfo(loop,
        test.value("accessToken",std::string("access")),profiles);
    else operation=auth::refresh::refreshOAuthToken(loop,"refresh",
      scopes,services);
    Json value;
    bool done=false;
    operation.then([&](const Json* result,const std::exception_ptr& error){
      if(error){
        try{std::rethrow_exception(error);}
        catch(const std::exception& caught){value={{"error",caught.what()}};}
      }else value=*result;
      done=true;
    });
    for(int attempt=0;attempt<100&&!done;++attempt)loop.runReady();
    if(!done)throw std::runtime_error("OAuth refresh did not settle");
    std::cout<<value::stringify(Json{{"value",value},
      {"config",config},{"trace",trace}})<<'\n';
  }
}
