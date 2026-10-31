#include "settings/remote/Security.hpp"
#include <algorithm>
#include <stdexcept>
#include <unicode/ustring.h>

namespace batbox::settings::remote {
namespace {
std::string upperJs(const std::string& text){
  const auto input=value::utf16CodeUnits(text);
  UErrorCode error=U_ZERO_ERROR;
  const auto required=u_strToUpper(nullptr,0,
    reinterpret_cast<const UChar*>(input.data()),
    static_cast<int32_t>(input.size()),"root",&error);
  if(error!=U_BUFFER_OVERFLOW_ERROR&&U_FAILURE(error))
    throw std::runtime_error("Unicode uppercase failed");
  std::u16string output(static_cast<std::size_t>(required),u'\0');
  error=U_ZERO_ERROR;
  u_strToUpper(reinterpret_cast<UChar*>(output.data()),required,
    reinterpret_cast<const UChar*>(input.data()),
    static_cast<int32_t>(input.size()),"root",&error);
  if(U_FAILURE(error))throw std::runtime_error("Unicode uppercase failed");
  return value::fromUtf16(output);
}
bool nonemptyString(const Json& value){
  return value.is_string()&&!value.get_ref<const std::string&>().empty();
}
Json comparable(const DangerousSettings& value){
  Json result={{"shellSettings",value.shellSettings},
    {"envVars",value.envVars}};
  if(value.hooks)result["hooks"]=*value.hooks;
  return result;
}
}
DangerousSettings extractDangerousSettings(const std::optional<Json>& settings,
    const DangerousConfig& config){
  DangerousSettings result;
  if(!settings||settings->is_null())return result;
  if(!settings->is_object())return result;
  for(const auto& key:config.shellSettings){
    if(settings->contains(key)&&nonemptyString((*settings)[key]))
      result.shellSettings[key]=(*settings)[key];
  }
  if(settings->contains("env")&&((*settings)["env"].is_object()||
      (*settings)["env"].is_array())){
    const auto& env=(*settings)["env"];
    for(const auto& key:value::objectKeys(env)){
      const auto& value=env.is_array()?env.at(std::stoull(key)):env.at(key);
      if(nonemptyString(value)&&!config.safeEnvVars.contains(upperJs(key)))
        result.envVars[key]=value;
    }
  }
  if(settings->contains("hooks")&&
      ((*settings)["hooks"].is_object()||(*settings)["hooks"].is_array())&&
      !value::objectKeys((*settings)["hooks"]).empty()){
    result.hasHooks=true;result.hooks=(*settings)["hooks"];
  }
  return result;
}
bool hasDangerousSettings(const DangerousSettings& dangerous){
  return !dangerous.shellSettings.empty()||!dangerous.envVars.empty()||
    dangerous.hasHooks;
}
bool hasDangerousSettingsChanged(const std::optional<Json>& oldSettings,
    const std::optional<Json>& newSettings,const DangerousConfig& config){
  const auto oldDangerous=extractDangerousSettings(oldSettings,config);
  const auto newDangerous=extractDangerousSettings(newSettings,config);
  if(!hasDangerousSettings(newDangerous))return false;
  if(!hasDangerousSettings(oldDangerous))return true;
  return value::stringify(comparable(oldDangerous))!=
    value::stringify(comparable(newDangerous));
}
std::vector<std::string> formatDangerousSettingsList(
    const DangerousSettings& dangerous){
  auto result=value::objectKeys(dangerous.shellSettings);
  const auto env=value::objectKeys(dangerous.envVars);
  result.insert(result.end(),env.begin(),env.end());
  if(dangerous.hasHooks)result.push_back("hooks");
  return result;
}
event_loop::Deferred<SecurityDecision> checkManagedSettingsSecurity(
    event_loop::Loop& loop,const std::optional<Json>& stashedSettings,
    const std::optional<Json>& newSettings,const SecurityServices& services){
  if(!services.isInteractive||!services.showDialog)
    throw std::invalid_argument("Missing managed settings security binding");
  event_loop::Deferred<SecurityDecision> output(loop);
  try{
    if(!newSettings||!hasDangerousSettings(
        extractDangerousSettings(newSettings,services.config))||
        !hasDangerousSettingsChanged(stashedSettings,newSettings,
          services.config)||!services.isInteractive()){
      output.resolve(SecurityDecision::NoCheckNeeded);return output;
    }
    services.showDialog(*newSettings).then([output](
        const SecurityDecision* decision,const std::exception_ptr& error){
      if(error)output.reject(error);else output.resolve(*decision);
    });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
bool handleSecurityCheckResult(SecurityDecision result,
    const SecurityServices& services){
  if(result==SecurityDecision::Rejected){
    if(!services.gracefulShutdown)
      throw std::invalid_argument("Missing managed settings shutdown binding");
    services.gracefulShutdown(1);return false;
  }
  return true;
}
}
