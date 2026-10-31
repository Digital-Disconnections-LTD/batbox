#include "auth/oauth_client/ProfileAccount.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>

int main(){
  using namespace batbox;
  const Json defaultProfile={{"account",{{"uuid","account-profile"},
      {"email","p@example.test"},{"display_name","Pat"},
      {"created_at","2025-01-01"}}},
    {"organization",{{"uuid","org-profile"},
      {"has_extra_usage_enabled",true},{"billing_type","subscription"},
      {"subscription_created_at","2025-02-01"}}}};
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    event_loop::Loop loop;
    Json trace=Json::array(),value=nullptr,error=nullptr;
    Json config=test.value("config",Json::object());
    auth::oauth_client::ProfileAccountServices services;
    services.environment=[&](const std::string& key)->Json{
      trace.push_back(Json::array({"env",key}));
      const char* field=nullptr;
      if(key==services.accountUuidEnv)field="envAccount";
      else if(key==services.userEmailEnv)field="envEmail";
      else if(key==services.organizationUuidEnv)field="envOrg";
      return field&&test.contains(field)?test[field]:Json(nullptr);
    };
    services.getGlobalConfig=[&]{trace.push_back("globalConfig");
      return config;};
    services.saveGlobalConfig=[&](const std::function<Json(const Json&)>&
        modifier){trace.push_back("saveGlobalConfig");
      config=modifier(config);};
    services.getOAuthTokens=[&]()->std::optional<Json>{
      trace.push_back("tokens");
      if(test.contains("tokens")&&!test["tokens"].is_null())
        return test["tokens"];
      return std::nullopt;};
    services.hasProfileScope=[&]{trace.push_back("scope");
      return test.value("scope",true);};
    services.isSubscriber=[&]{trace.push_back("subscriber");
      return test.value("subscriber",true);};
    services.checkAndRefresh=[&]{trace.push_back("refresh");
      event_loop::Deferred<event_loop::Unit> done(loop);
      if(test.value("failRefresh",false))done.reject(
        std::make_exception_ptr(std::runtime_error("refresh failed")));
      else done.resolve({});
      return done;};
    services.getRawProfile=[&](const std::string& token){
      trace.push_back(Json::array({"profile",token}));
      event_loop::Deferred<std::optional<Json>> result(loop);
      if(test.value("failProfile",false))result.reject(
        std::make_exception_ptr(std::runtime_error("profile failed")));
      else if(test.contains("profile")&&test["profile"].is_null())
        result.resolve(std::nullopt);
      else result.resolve(test.value("profile",defaultProfile));
      return result;};
    services.logForDebugging=[&](const std::string&){
      trace.push_back(Json::array({"debug",
        "OAuth profile fetch succeeded, overriding env var account info",
        Json{{"level","info"}}}));};
    const auto op=test.value("op",std::string("populate"));
    bool done=false;
    if(op=="store"){
      try{auth::oauth_client::storeOAuthAccountInfo(test.value("fields",
        Json{{"accountUuid","acct"},{"emailAddress","a@example.test"},
          {"organizationUuid","org"}}),services);}
      catch(const std::exception& caught){error=caught.what();}
      done=true;
    }else if(op=="uuid")
      auth::oauth_client::getOrganizationUUID(loop,services).then(
        [&](const std::optional<std::string>* result,
            const std::exception_ptr& caught){
          if(caught){try{std::rethrow_exception(caught);}
            catch(const std::exception& cause){error=cause.what();}}
          else if(*result)value=**result;
          done=true;});
    else auth::oauth_client::populateOAuthAccountInfoIfNeeded(loop,services)
      .then([&](const bool* result,const std::exception_ptr& caught){
        if(caught){try{std::rethrow_exception(caught);}
          catch(const std::exception& cause){error=cause.what();}}
        else value=*result;
        done=true;});
    for(int i=0;i<100&&!done;++i)loop.runReady();
    if(!done)throw std::runtime_error("OAuth profile operation did not settle");
    std::cout<<value::stringify(Json{{"value",value},{"error",error},
      {"trace",trace},{"config",config}})<<'\n';
  }
}
