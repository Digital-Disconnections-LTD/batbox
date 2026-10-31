#include "auth/OAuthTokens.hpp"
#include <iostream>
#include <stdexcept>

int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    event_loop::Loop loop;
    Json values=Json::array(),trace=Json::array();
    bool bare=test.value("bare",false),storageError=
      test.value("storageError",false);
    Json env=test.value("env",Json(nullptr)),fd=test.value("fd",Json(nullptr));
    Json stored=test.value("stored",Json(nullptr));
    const auto stringOrNull=[](const Json& value)->std::optional<std::string>{
      return value.is_string()?std::optional(value.get<std::string>()):
        std::nullopt;
    };
    auth::OAuthTokenServices services;
    services.isBareMode=[&](){trace.push_back("bare");return bare;};
    services.environment=[&](const std::string&){
      trace.push_back("env");return stringOrNull(env);};
    services.oauthTokenFromFd=[&](){
      trace.push_back("fd");return stringOrNull(fd);};
    services.secureStorageRead=[&]()->std::optional<Json>{
      trace.push_back("secure");trace.push_back("read");
      if(storageError)throw std::runtime_error("storage failed");
      return Json{{"batboxOauth",stored}};
    };
    services.secureStorageReadAsync=[&](){
      trace.push_back("secure");trace.push_back("readAsync");
      event_loop::Deferred<std::optional<Json>> output(loop);
      if(storageError)output.reject(std::make_exception_ptr(
        std::runtime_error("storage failed")));
      else output.resolve(Json{{"batboxOauth",stored}});
      return output;
    };
    services.logError=[&](const std::string& message){
      trace.push_back(Json::array({"error",message}));};
    services.clearKeychainStash=[&](){trace.push_back("clearKeychain");};
    auth::OAuthTokens tokens(loop,services);
    for(const auto& action:test["actions"]){
      if(action.is_string()){
        const auto op=action.get<std::string>();
        if(op=="sync"){
          const auto value=tokens.getBatboxOAuthTokens();
          values.push_back(value?*value:Json(nullptr));
        }else if(op=="async"){
          bool done=false;
          tokens.getBatboxOAuthTokensAsync().then([&](
              const std::optional<Json>* value,const std::exception_ptr& error){
            if(error)std::rethrow_exception(error);
            values.push_back(*value?**value:Json(nullptr));done=true;
          });
          for(int attempt=0;attempt<100&&!done;++attempt)loop.runReady();
          if(!done)throw std::runtime_error("OAuth async did not settle");
        }else if(op=="clear")tokens.clearOAuthTokenStash();
        else throw std::runtime_error("Unknown action");
      }else{
        if(action.contains("bare"))bare=action["bare"].get<bool>();
        if(action.contains("env"))env=action["env"];
        if(action.contains("fd"))fd=action["fd"];
        if(action.contains("stored"))stored=action["stored"];
        if(action.contains("storageError"))
          storageError=action["storageError"].get<bool>();
      }
    }
    std::cout<<value::stringify(Json{{"values",values},{"trace",trace}})
      <<'\n';
  }
}
