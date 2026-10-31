#include "settings/remote/Security.hpp"
#include <iostream>

int main(){
  using namespace batbox;
  using namespace batbox::settings::remote;
  const DangerousConfig config{{"apiKeyHelper","awsAuthRefresh"},
    {"BATBOX_SAFE"}};
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    const auto setting=test.contains("settings")&&
      !test["settings"].is_null()
      ?std::optional(test["settings"]):std::nullopt;
    const auto old=test.contains("old")&&!test["old"].is_null()
      ?std::optional(test["old"]):std::nullopt;
    const auto dangerous=extractDangerousSettings(setting,config);
    Json data={{"shellSettings",dangerous.shellSettings},
      {"envVars",dangerous.envVars},{"hasHooks",dangerous.hasHooks}};
    if(dangerous.hooks)data["hooks"]=*dangerous.hooks;
    std::cout<<value::stringify(Json{{"dangerous",data},
      {"has",hasDangerousSettings(dangerous)},
      {"changed",hasDangerousSettingsChanged(old,setting,config)},
      {"names",formatDangerousSettingsList(dangerous)}})<<'\n';
  }
}
