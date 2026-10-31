#pragma once

#include "application/Config.hpp"
#include "application_model/LocalModelService.hpp"
#include "services/compact/autoCompact.hpp"
#include <cmath>
#include <cstdlib>

namespace batbox::application_engine {

inline bool autoCompactSetting(const Json& appState){
  const auto& selected=value::property(value::property(appState,"settings"),
    "autoCompactEnabled");
  return !selected.is_boolean()||selected.get<bool>();
}

inline AutoCompactDependencies autoCompactDependencies(
    const application::Config& config,bool settingEnabled,
    const std::string& modelName={},
    const application_model::LocalModelService* modelService=nullptr,
    const Json& liveSettings=Json::object()){
  AutoCompactDependencies result;
  result.contextWindow=static_cast<double>(config.contextWindow);
  result.maxOutputTokens=static_cast<double>(config.maxOutputTokens);
  if(modelService){
    const auto profile=modelService->findProfile(modelName);
    if(profile){
      if(profile->contextTokens>0)
        result.contextWindow=profile->contextTokens;
      if(profile->maxOutputTokens>0)
        result.maxOutputTokens=profile->maxOutputTokens;
    }
  }
  result.autoCompactEnabled=settingEnabled;
  for(const auto* key:{"BATBOX_AUTO_COMPACT_WINDOW",
      "BATBOX_AUTOCOMPACT_PCT_OVERRIDE","BATBOX_BLOCKING_LIMIT_OVERRIDE",
      "BATBOX_DISABLE_COMPACT","BATBOX_DISABLE_AUTO_COMPACT"})
    if(const auto* value=std::getenv(key))result.env[key]=value;
  std::size_t compactAt=config.compactAt;
  const auto& threshold=value::property(liveSettings,"compactAt");
  if(threshold.is_number_integer()&&threshold.get<double>()>0)
    compactAt=threshold.get<std::size_t>();
  if(!result.env.contains("BATBOX_AUTOCOMPACT_PCT_OVERRIDE")){
    const double effective=getEffectiveContextWindowSize(result);
    if(effective>0){
      const double percentage=std::nextafter(
        100.0*static_cast<double>(compactAt)/effective,INFINITY);
      result.env["BATBOX_AUTOCOMPACT_PCT_OVERRIDE"]=
        value::numberToString(percentage);
    }
  }
  return result;
}

} // namespace batbox::application_engine
