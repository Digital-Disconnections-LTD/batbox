#include "settings/persistence/ApplyChange.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

int main(){
  using namespace batbox;
  using namespace batbox::settings;
  using namespace batbox::settings::persistence;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    Json trace=Json::array(),value=nullptr;
    ApplyChangeServices services;
    services.internalUser=test.value("internal",false);
    services.localBatEntrypoint=test.value("localAgent",false);
    services.getInitialSettings=[&](){trace.push_back("settings");
      return test["next"];};
    services.debug=[&](const std::string& text){trace.push_back({"log",text});};
    services.loadAllPermissionRulesFromDisk=[&](){trace.push_back("rules");
      return test["rules"];};
    services.updateHooksConfigSnapshot=[&](){trace.push_back("hooks");};
    services.syncPermissionRulesFromDisk=[&](const Json& context,
        const Json& rules){
      trace.push_back("sync");Json next=context;next["synced"]=rules.size();
      return next;
    };
    services.findOverlyBroadBashPermissions=[&](const Json&,
        const Json& extra){
      trace.push_back({"broad",extra.size()});return test["broad"];};
    services.removeDangerousPermissions=[&](const Json& context,
        const Json& broad){
      trace.push_back("strip");Json next=context;
      next["stripped"]=broad.size();return next;
    };
    services.isBypassPermissionsModeDisabled=[&](){
      trace.push_back("bypassCheck");return test.value("bypassDisabled",false);
    };
    services.createDisabledBypassPermissionsContext=[&](const Json& context){
      trace.push_back("disableBypass");Json next=context;
      next["bypassDisabled"]=true;return next;
    };
    services.transitionPlanAutoMode=[&](const Json& context){
      trace.push_back("transition");Json next=context;
      next["transitioned"]=true;return next;
    };
    applySettingsChange(sourceFromKey(test["source"].get<std::string>()),
      [&](std::function<Json(const Json&)> update){
        trace.push_back("setState");value=update(test["prev"]);
      },services);
    std::cout<<value::stringify({{"value",value},{"trace",trace}})<<'\n';
  }
}
