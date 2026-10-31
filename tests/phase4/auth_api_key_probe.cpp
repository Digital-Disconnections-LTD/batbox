#include "auth/ApiKey.hpp"
#include "auth/LegacyKey.hpp"
#include <iostream>
#include <stdexcept>

int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    Json test=value::parse(line),state=test;
    Json trace=Json::array(),values=Json::array();
    const auto field=[&](const char* name)->Json{
      return state.contains(name)?state[name]:Json(nullptr);
    };
    const auto value=[&](const char* name)->std::string{
      const auto item=field(name);
      return item.is_string()?item.get<std::string>():std::string{};
    };
    auth::LegacyKeyServices legacy;
    legacy.bareMode=[&](){trace.push_back("bare");
      return field("bare")==true;};
    legacy.platform=[&](){return value("platform").empty()
      ?std::string("linux"):value("platform");};
    legacy.legacyKeychainPrefetch=[&]()->std::optional<Json>{
      trace.push_back("prefetch");const auto item=field("prefetch");
      return item.is_null()?std::nullopt:std::optional(item);};
    legacy.keychainServiceName=[&](){trace.push_back("service");
      return std::string("batbox-auth");};
    legacy.executeSync=[&](const std::string& command)
        ->std::optional<std::string>{
      trace.push_back(Json::array({"exec",command}));
      if(field("execError")==true)throw std::runtime_error("keychain failed");
      const auto result=value("execResult");
      return result.empty()?std::nullopt:std::optional(result);
    };
    legacy.logError=[&](const std::string& message){
      trace.push_back(Json::array({"error",message}));};
    legacy.globalConfig=[&](){trace.push_back("config");
      const auto result=field("config");
      return result.is_null()?Json::object():result;};
    auth::LegacyKeyReader login(legacy);
    auth::ApiKeyServices services;
    services.environment=[&](const std::string& key)->Json{
      const char* name=nullptr;
      if(key=="BATBOX_API_KEY")name="apiKey";
      else if(key=="BATBOX_OAUTH_TOKEN")name="oauthToken";
      else if(key=="BATBOX_OAUTH_TOKEN_FILE_DESCRIPTOR")name="oauthFd";
      else if(key=="CI")name="ci";
      else if(key=="NODE_ENV")name="nodeEnv";
      trace.push_back(Json::array({"env",key}));
      return name?field(name):Json(nullptr);
    };
    services.bareMode=legacy.bareMode;
    services.runningOnHomespace=[&](){trace.push_back("homespace");
      return field("homespace")==true;};
    services.preferThirdPartyAuthentication=[&](){
      trace.push_back("thirdParty");return field("thirdParty")==true;};
    services.flagSettings=[&](){trace.push_back(Json::array(
      {"source","flagSettings"}));
      const auto result=field("flagSettings");
      return result.is_null()?Json::object():result;};
    services.mergedSettings=[&](){trace.push_back("merged");
      const auto result=field("mergedSettings");
      return result.is_null()?Json::object():result;};
    services.globalConfig=legacy.globalConfig;
    services.apiKeyFromFd=[&]()->std::optional<std::string>{
      trace.push_back("fd");const auto result=value("fd");
      return result.empty()?std::nullopt:std::optional(result);};
    services.apiKeyFromHelperStashed=[&](){trace.push_back("helperCached");
      return field("helperCached");};
    services.loginManagedKey=[&](){return login.getLoginManagedKey();};
    const auto show=[](const auth::ApiKeyResult& result){
      return Json{{"key",result.key},{"source",result.source}};};
    for(const auto& action:test["actions"]){
      if(action.is_object()){
        for(auto it=action.begin();it!=action.end();++it)state[it.key()]=it.value();
        continue;
      }
      const auto op=action.get<std::string>();
      try{
        if(op=="key")values.push_back(auth::getBatboxApiKey(services));
        else if(op=="has")values.push_back(auth::hasBatboxApiKeyAuth(services));
        else if(op=="source")values.push_back(show(
          auth::getBatboxApiKeyWithSource(services)));
        else if(op=="skip")values.push_back(show(
          auth::getBatboxApiKeyWithSource(services,true)));
        else if(op=="helper")values.push_back(
          auth::getConfiguredApiKeyHelper(services));
        else if(op=="login"){
          const auto result=login.getLoginManagedKey();
          values.push_back(result?show(*result):Json(nullptr));
        }else if(op=="clearLogin")login.clearStash();
        else throw std::runtime_error("Unknown action");
      }catch(const std::exception& error){
        values.push_back(Json{{"error",error.what()}});
      }
    }
    std::cout<<value::stringify(Json{{"values",values},{"trace",trace}})
      <<'\n';
  }
}
