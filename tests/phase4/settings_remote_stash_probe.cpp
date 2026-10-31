#include "settings/remote/Eligibility.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>

int main(){
  using namespace batbox;
  using namespace batbox::settings::remote;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    Json trace=Json::array(),values=Json::array();
    const auto files=test.value("files",Json::object());
    SyncStashServices stashServices;
    stashServices.configHome=[&](){
      trace.push_back({"configHome"});return "/config";};
    stashServices.readFileSync=[&](const std::string& file){
      trace.push_back({"read",file});
      if(!files.contains(file))throw std::runtime_error("ENOENT");
      return files[file].get<std::string>();
    };
    stashServices.resetMergedSettingsStash=[&](){
      trace.push_back({"resetMerged"});};
    SyncStash stash(stashServices);
    EligibilityServices services;
    services.provider=[&](){trace.push_back({"provider"});
      return test.value("provider",std::string("firstParty"));};
    services.isPrimaryBaseUrl=[&](){trace.push_back({"baseUrl"});
      return test.value("baseUrl",true);};
    services.entrypoint=[&](){
      return test.value("entrypoint",std::string{});};
    services.oauthTokens=[&](){trace.push_back({"oauth"});
      return test.value("tokens",Json(nullptr));};
    services.inferenceScope="inference";
    services.apiKeyWithoutHelper=[&]()->std::optional<std::string>{
      trace.push_back({"apiKey",Json{{"skipRetrievingKeyFromApiKeyHelper",true}}});
      if(test.value("keyError",false))throw std::runtime_error("key failed");
      if(!test.contains("key")||test["key"].is_null())return std::nullopt;
      return test["key"].get<std::string>();
    };
    Eligibility eligibility(stash,services);
    for(const auto& action:test["actions"]){
      const auto op=action.get<std::string>();
      if(op=="eligible")values.push_back(
        eligibility.isRemoteManagedSettingsEligible());
      else if(op=="read"){
        const auto value=stash.getRemoteManagedSettingsSyncFromStash();
        values.push_back(value?*value:Json(nullptr));
      }else if(op=="path")values.push_back(stash.getSettingsPath());
      else if(op=="reset")eligibility.resetSyncStash();
      else if(op=="setTrue")stash.setEligibility(true);
      else if(op=="setFalse")stash.setEligibility(false);
      else if(op=="session")stash.setFlightStash(test.value("session",Json(nullptr)));
    }
    std::cout<<value::stringify(Json{{"values",values},{"trace",trace}})<<'\n';
  }
}
