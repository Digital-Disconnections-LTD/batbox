#include "auth/storage/KeychainHelpers.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>

int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    Json trace=Json::array(),values=Json::array();
    auth::storage::KeychainServices services;
    services.configHome=[&](){trace.push_back("home");
      return test.value("home",std::string("/config"));};
    services.configDirOverride=[&]()->std::optional<std::string>{
      if(!test.contains("override")||test["override"].is_null())
        return std::nullopt;
      return test["override"].get<std::string>();
    };
    services.userEnv=[&]()->std::optional<std::string>{
      if(!test.contains("user")||test["user"].is_null())
        return std::nullopt;
      return test["user"].get<std::string>();
    };
    services.osUsername=[&](){
      trace.push_back("userInfo");
      if(test.value("userError",false))
        throw std::runtime_error("user failed");
      return test.value("osUser",std::string("system-user"));
    };
    services.nowMs=[&](){trace.push_back("now");
      return test.value("now",1234.0);};
    services.oauthFileSuffix=[&](){trace.push_back("oauth");
      return std::string("-oauth");};
    auth::storage::KeychainState state(services);
    for(const auto& action:test.value("actions",Json::array())){
      const auto op=action.get<std::string>();
      if(op=="service"){
        const auto result=state.serviceName(
          test.value("suffix",std::string()));
        values.push_back(result);
      }else if(op=="username")values.push_back(state.username());
      else if(op=="clear"){
        state.clear();values.push_back("cleared");
      }else if(op=="prime"){
        std::optional<std::string> stdoutText;
        if(test.contains("stdout")&&!test["stdout"].is_null())
          stdoutText=test["stdout"].get<std::string>();
        state.prime(stdoutText);values.push_back("primed");
      }else if(op=="state")values.push_back(Json{
        {"data",state.data?*state.data:Json(nullptr)},
        {"cachedAt",state.stashedAt},{"generation",state.generation},
        {"readInFlight",nullptr},
        {"ttl",auth::storage::KeychainState::stashTtlMs}});
    }
    std::cout<<value::stringify(Json{{"values",values},
      {"trace",trace}})<<'\n';
  }
}
