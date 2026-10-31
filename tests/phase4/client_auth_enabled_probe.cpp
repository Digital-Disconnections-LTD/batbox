#include "client_factory/AuthEnabled.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    Json trace=Json::array();
    client_factory::AuthEnabledServices services;
    services.environment=[&](const std::string& key)->Json{
      trace.push_back(Json::array({"env",key}));
      const auto& k=services.keys;
      const char* field=nullptr;
      if(key==k.remote)field="remote";
      else if(key==k.unixSocket)field="unix";
      else if(key==k.oauthToken)field="oauth";
      else if(key==k.useBedrock)field="bedrock";
      else if(key==k.useVertex)field="vertex";
      else if(key==k.useFoundry)field="foundry";
      else if(key==k.authToken)field="authToken";
      else if(key==k.apiKeyFd)field="apiKeyFd";
      else if(key==k.entrypoint)
        return test.value("desktop",false)?Json("batbox-desktop"):
          Json("cli");
      return field&&test.contains(field)?test[field]:Json(nullptr);
    };
    services.bareMode=[&]{trace.push_back("bare");
      return test.value("bare",false);};
    services.mergedSettings=[&]{trace.push_back("settings");
      return test.value("settings",Json(nullptr));};
    services.apiKeyWithSource=[&](bool skip){
      trace.push_back(Json::array({"apiKeySource",skip}));
      auto source=test.value("keySource",std::string("none"));
      if(source=="external")source="BATBOX_API_KEY";
      return auth::ApiKeyResult{nullptr,source};
    };
    std::cout<<value::stringify(Json{{"value",
      client_factory::isBatboxAuthEnabled(services)},
      {"trace",trace}})<<'\n';
  }
}
