#include "auth/Provider.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    Json trace=Json::array();
    auth::ProviderServices services;
    services.primaryHost="api.anthropic.com";
    services.stagingHost="api-staging.anthropic.com";
    services.keys={"BATBOX_USE_BEDROCK","BATBOX_USE_VERTEX",
      "BATBOX_USE_FOUNDRY","ANTHROPIC_BASE_URL"};
    services.environment=[&](const std::string& key){
      trace.push_back({"env",key});
      if(key==services.keys.useBedrock)return test.value("bedrock",Json(nullptr));
      if(key==services.keys.useVertex)return test.value("vertex",Json(nullptr));
      if(key==services.keys.useFoundry)return test.value("foundry",Json(nullptr));
      return test.value("baseUrl",Json(nullptr));
    };
    services.internalUser=[&](){trace.push_back({"env","USER_TYPE"});
      return test.value("internal",false);};
    const auto provider=auth::providerName(auth::getAPIProvider(services));
    const auto allowed=auth::isFirstPartyBaseUrl(services);
    std::cout<<value::stringify(Json{{"provider",provider},
      {"allowed",allowed},{"trace",trace}})<<'\n';
  }
}
