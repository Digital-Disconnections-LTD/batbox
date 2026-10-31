#include "settings/schema/Internal.hpp"
#include <array>

namespace batbox::settings::schema {
namespace {
void permissionArray(const Json& input,const char* key,Json& output,
  Json& issues,const PermissionRuleServices& rules){
  if(!input.contains(key))return;
  const auto& value=input[key];
  if(!value.is_array()){
    detail::invalidType(issues,Json::array({key}),"array",value);return;
  }
  Json parsed=Json::array();
  bool valid=true;
  for(std::size_t i=0;i<value.size();++i){
    const auto& item=value[i];
    if(!item.is_string()){
      detail::invalidType(issues,Json::array({key,i}),"string",item);
      valid=false;continue;
    }
    const auto text=item.get<std::string>();
    const auto result=validatePermissionRule(text,rules);
    if(!result.valid){
      auto message=result.error.value_or("");
      if(result.suggestion)message+=". "+*result.suggestion;
      if(result.examples&&!result.examples->empty()){
        message+=". Examples: ";
        for(std::size_t n=0;n<result.examples->size();++n){
          if(n)message+=", ";
          message+=(*result.examples)[n];
        }
      }
      issues.push_back({{"code","custom"},{"message",message},
        {"params",Json{{"received",text}}},
        {"path",Json::array({key,i})}});
      valid=false;
    }
    parsed.push_back(item);
  }
  if(valid)output[key]=parsed;
}
void enumField(const Json& input,const char* key,
  const std::vector<std::string>& values,Json& output,Json& issues){
  if(!input.contains(key))return;
  const auto& item=input[key];
  if(item.is_string())for(const auto& value:values)
    if(item.get<std::string>()==value){output[key]=item;return;}
  detail::invalidValue(issues,Json::array({key}),values);
}
}
SchemaResult permissions(const Json& input,const Config& config,
  const PermissionRuleServices& rules){
  Json issues=Json::array();
  if(!input.is_object()){
    detail::invalidType(issues,Json::array(),"object",input);
    return {false,nullptr,issues};
  }
  Json result=Json::object();
  for(const auto key:{"allow","deny","ask"})
    permissionArray(input,key,result,issues,rules);
  std::vector<std::string> modes={"acceptEdits","bypassPermissions",
    "default","dontAsk","plan"};
  if(config.transcriptClassifier)modes.push_back("auto");
  enumField(input,"defaultMode",modes,result,issues);
  enumField(input,"disableBypassPermissionsMode",{"disable"},
    result,issues);
  if(config.transcriptClassifier)
    enumField(input,"disableAutoMode",{"disable"},result,issues);
  detail::stringArrayField(input,"additionalDirectories",result,issues);
  constexpr std::array<const char*,7> known={"allow","deny","ask",
    "defaultMode","disableBypassPermissionsMode","disableAutoMode",
    "additionalDirectories"};
  for(auto it=input.begin();it!=input.end();++it){
    bool recognized=false;
    for(const auto key:known)if(it.key()==key){recognized=true;break;}
    if(it.key()=="disableAutoMode"&&!config.transcriptClassifier)
      recognized=false;
    if(!recognized)result[it.key()]=it.value();
  }
  return {issues.empty(),issues.empty()?result:Json(nullptr),issues};
}
}
