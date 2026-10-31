#include "settings/PermissionRules.hpp"

namespace batbox::settings {
Json filterInvalidPermissionRules(Json& data,const std::string& filePath,
  const PermissionRuleServices& services){
  Json warnings=Json::array();
  if(!(data.is_object()||data.is_array()))return warnings;
  if(!data.is_object()||!data.contains("permissions")||
     !data["permissions"].is_object())return warnings;
  auto& permissions=data["permissions"];
  for(const auto key:{"allow","deny","ask"}){
    if(!permissions.contains(key)||!permissions[key].is_array())continue;
    Json kept=Json::array();
    for(const auto& rule:permissions[key]){
      if(!rule.is_string()){
        warnings.push_back({{"file",filePath},
          {"path",std::string("permissions.")+key},
          {"message",std::string("Non-string value in ")+key+
            " array was removed"},{"invalidValue",rule}});
        continue;
      }
      const auto name=rule.get<std::string>();
      const auto result=validatePermissionRule(name,services);
      if(!result.valid){
        auto message="Invalid permission rule \""+name+"\" was skipped";
        if(result.error)message+=": "+*result.error;
        if(result.suggestion)message+=". "+*result.suggestion;
        warnings.push_back({{"file",filePath},
          {"path",std::string("permissions.")+key},
          {"message",message},{"invalidValue",name}});
        continue;
      }
      kept.push_back(rule);
    }
    permissions[key]=kept;
  }
  return warnings;
}
}
