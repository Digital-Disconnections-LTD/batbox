#include "auth/oauth_client/AccountActions.hpp"
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
    Json globalConfig=test.value("config",
      Json{{"oauthAccount",Json{{"email","a@example.test"}}}});
    auth::oauth_client::AccountServices services;
    services.getConfig=[&]{trace.push_back("config");
      client_factory::oauth_config::Config result;
      result.rolesUrl="https://oauth.example/roles";
      result.apiKeyUrl="https://oauth.example/key";
      return result;};
    auto response=[&]{
      event_loop::Deferred<auth::refresh::HttpResponse> result(loop);
      if(test.value("throwHttp",false))result.reject(
        std::make_exception_ptr(std::runtime_error("network failure")));
      else result.resolve({test.value("status",200),
        test.value("statusText",std::string("OK")),
        test.value("data",test.value("op",std::string())=="roles"?
          Json{{"organization_role","admin"},{"workspace_role","owner"},
            {"organization_name","Example"}}:
          Json{{"raw_key","key-1"}})});
      return result;
    };
    services.httpGet=[&](const std::string& url,const Json& headers){
      trace.push_back(Json::array({"get",url,Json{{"headers",headers}}}));
      return response();};
    services.httpPost=[&](const std::string& url,const Json& body,
        const Json& headers){
      trace.push_back(Json::array({"post",url,body,
        Json{{"headers",headers}}}));
      return response();};
    services.getGlobalConfig=[&]{trace.push_back("globalConfig");
      return globalConfig;};
    services.saveGlobalConfig=[&](const std::function<Json(const Json&)>&
        modifier){trace.push_back("saveGlobalConfig");
      globalConfig=modifier(globalConfig);};
    services.saveApiKey=[&](const std::string& key){
      trace.push_back(Json::array({"saveApiKey",key}));
      event_loop::Deferred<event_loop::Unit> saved(loop);
      if(test.value("failSave",false))saved.reject(
        std::make_exception_ptr(std::runtime_error("save failed")));
      else saved.resolve({});
      return saved;};
    bool done=false;
    if(test.value("op",std::string())=="roles")
      auth::oauth_client::fetchAndStoreUserRoles(loop,"access-1",services)
        .then([&](const event_loop::Unit*,const std::exception_ptr& caught){
          if(caught){try{std::rethrow_exception(caught);}
            catch(const std::exception& cause){error=cause.what();}}
          done=true;});
    else auth::oauth_client::createAndStoreApiKey(loop,"access-1",services)
      .then([&](const std::optional<std::string>* result,
          const std::exception_ptr& caught){
        if(caught){try{std::rethrow_exception(caught);}
          catch(const std::exception& cause){error=cause.what();}}
        else if(*result)value=**result;
        done=true;});
    for(int i=0;i<100&&!done;++i)loop.runReady();
    if(!done)throw std::runtime_error("OAuth account action did not settle");
    std::cout<<value::stringify(Json{{"value",value},{"error",error},
      {"trace",trace},{"config",globalConfig}})<<'\n';
  }
}
