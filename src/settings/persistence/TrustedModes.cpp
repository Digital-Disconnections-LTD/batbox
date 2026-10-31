#include "settings/persistence/TrustedModes.hpp"
#include <stdexcept>

namespace batbox::settings::persistence {
namespace {
std::optional<Json> maybeProperty(const Json& settings,const char* key){
  if(settings.is_object()&&settings.contains(key))return settings[key];
  return std::nullopt;
}
Json property(const Json& settings,const char* key){
  return maybeProperty(settings,key).value_or(Json(nullptr));
}
void need(const TrustedModeServices& services){
  if(!services.getSettingsForSource)
    throw std::invalid_argument("Missing trusted settings source reader");
}
bool stringArray(const Json& value){
  if(!value.is_array())return false;
  for(const auto& item:value)if(!item.is_string())return false;
  return true;
}
bool validAutoMode(const Json& value){
  if(!value.is_object())return false;
  for(const auto key:{"allow","soft_deny","deny","environment"})
    if(value.contains(key)&&!stringArray(value[key]))return false;
  return true;
}
std::string debugValue(const std::optional<Json>& value){
  if(!value)return "undefined";
  if(value->is_null())return "null";
  if(value->is_string())return value->get<std::string>();
  if(value->is_boolean())return value->get<bool>()?"true":"false";
  if(value->is_object())return "[object Object]";
  if(value->is_array()){
    std::string result;
    for(std::size_t i=0;i<value->size();++i){
      if(i)result+=',';
      if(!(*value)[i].is_null())result+=debugValue((*value)[i]);
    }
    return result;
  }
  return value::stringify(*value);
}
}
bool hasSkipDangerousModePermissionPoetry(
  const TrustedModeServices& services){
  need(services);
  for(const auto source:{Source::User,Source::Local,Source::Flag,
      Source::Policy})
    if(value::truthy(property(services.getSettingsForSource(source),
      "skipDangerousModePermissionPrompt")))return true;
  return false;
}
bool hasAutoModeOptIn(const TrustedModeServices& services){
  if(!services.transcriptClassifier)return false;
  need(services);
  if(!services.debug)
    throw std::invalid_argument("Missing trusted settings debug logger");
  const auto user=maybeProperty(services.getSettingsForSource(Source::User),
    "skipAutoPermissionPrompt");
  const auto local=maybeProperty(services.getSettingsForSource(Source::Local),
    "skipAutoPermissionPrompt");
  const auto flag=maybeProperty(services.getSettingsForSource(Source::Flag),
    "skipAutoPermissionPrompt");
  const auto policy=maybeProperty(services.getSettingsForSource(Source::Policy),
    "skipAutoPermissionPrompt");
  const bool result=(user&&value::truthy(*user))||
    (local&&value::truthy(*local))||(flag&&value::truthy(*flag))||
    (policy&&value::truthy(*policy));
  services.debug(
    "[auto-mode] hasAutoModeOptIn="+std::string(result?"true":"false")+
    " skipAutoPermissionPrompt: user="+debugValue(user)+
    " local="+debugValue(local)+" flag="+debugValue(flag)+
    " policy="+debugValue(policy));
  return result;
}
bool getUseAutoModeDuringPlan(const TrustedModeServices& services){
  if(!services.transcriptClassifier)return true;
  need(services);
  for(const auto source:{Source::Policy,Source::Flag,Source::User,
      Source::Local})
    if(property(services.getSettingsForSource(source),
      "useAutoModeDuringPlan")==false)return false;
  return true;
}
std::optional<Json> getAutoModeConfig(const TrustedModeServices& services){
  if(!services.transcriptClassifier)return std::nullopt;
  need(services);
  Json allow=Json::array(),softDeny=Json::array(),environment=Json::array();
  for(const auto source:{Source::User,Source::Local,Source::Flag,
      Source::Policy}){
    const auto settings=services.getSettingsForSource(source);
    if(settings.is_null())continue;
    const auto& value=property(settings,"autoMode");
    if(!validAutoMode(value))continue;
    if(value.contains("allow"))
      for(const auto& x:value["allow"])allow.push_back(x);
    if(value.contains("soft_deny"))
      for(const auto& x:value["soft_deny"])softDeny.push_back(x);
    if(services.internalUser&&value.contains("deny"))
      for(const auto& x:value["deny"])softDeny.push_back(x);
    if(value.contains("environment"))
      for(const auto& x:value["environment"])environment.push_back(x);
  }
  if(allow.empty()&&softDeny.empty()&&environment.empty())return std::nullopt;
  Json result=Json::object();
  if(!allow.empty())result["allow"]=allow;
  if(!softDeny.empty())result["soft_deny"]=softDeny;
  if(!environment.empty())result["environment"]=environment;
  return result;
}
}
