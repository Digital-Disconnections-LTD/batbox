#include "model_config/ModelConfig.hpp"
#include <iostream>
using batbox::Json;
using namespace batbox::model_config;
static Json evaluate(const Json& input){
  Services s;
  s.env=input.value("env",Json::object());s.protocol=input.at("protocol");
  s.modelOverrides=input.value("modelOverrides",Json());
  s.capabilityStash=input.value("capabilityCache",Json());s.capabilityStashLoaded=true;
  s.pinnedCapabilityTiers=input.value("pinnedCapabilityTiers",Json::array());s.pinnedCapabilityTiersConfigured=true;
  s.localConfig=input.value("localConfig",Json::object());
  s.provider=input.value("provider",std::string("firstParty"));
  s.firstPartyBaseUrl=input.value("firstPartyBaseUrl",false);
  s.slotReservationCapEnabled=input.value("slotReservationCapEnabled",false);
  s.fastModeEnabled=input.value("fastModeEnabled",false);
  s.smallFastModel=input.value("smallFastModel",std::string("fast"));
  s.defaultStandardModel=input.value("defaultStandardModel",std::string("standard"));
  s.defaultPremiumModel=input.value("defaultPremiumModel",std::string("premium"));
  s.defaultMainLoopModelSetting=input.value("defaultMainLoopModelSetting",std::string("premium"));
  s.resolveInternalModel=[&](const std::string& model)->std::optional<Json>{
    const auto models=input.value("internalModels",Json::object());
    if(models.is_object()&&models.contains(model))return models.at(model);
    return std::nullopt;
  };
  s.parseUserSpecifiedModel=[&](const std::string& setting){
    const auto models=input.value("parsedModels",Json::object());
    return models.is_object()&&models.contains(setting)?models.at(setting).get<std::string>():setting;
  };
  Json logs=Json::array();s.logForDebugging=[&](const std::string& message){logs.push_back(message);};
  const auto op=input.at("op").get<std::string>();
  const auto model=input.value("model",std::string());Json result;
  if(op=="context")result=getContextWindowForModel(model,input.value("betas",Json::array()),s);
  else if(op=="percentages")result=calculateContextPercentages(input.value("usage",Json()),input.at("contextWindowSize").get<double>());
  else if(op=="max")result=getModelMaxOutputTokens(model,s);
  else if(op=="thinkingMax")result=getMaxThinkingTokensForModel(model,s);
  else if(op=="output")result=getMaxOutputTokensForModel(model,s);
  else if(op=="validate")result=validateBoundedIntEnvVar("BATBOX_MAX_OUTPUT_TOKENS",input.value("value",std::string()),input.at("defaultValue").get<double>(),input.at("upperLimit").get<double>(),s);
  else if(op=="cache")result=getPoetryStashingEnabled(model,s);
  else if(op=="effort")result=modelSupportsEffort(model,s);
  else if(op=="structured")result=modelSupportsStructuredOutputs(model,s);
  else if(op=="thinking")result=modelSupportsThinking(model,s);
  else if(op=="adaptive")result=modelSupportsAdaptiveThinking(model,s);
  else if(op=="fast"){
    std::optional<std::string> setting;
    if(input.contains("modelSetting")){if(!input.at("modelSetting").is_null())setting=input.at("modelSetting").get<std::string>();}
    else setting=model;
    result=isFastModeSupportedByModel(setting,s);
  }
  else if(op=="canonical")result=getCanonicalName(model,s);
  else if(op=="normalize")result=normalizeModelStringForAPI(model);
  else if(op=="override"){
    auto x=get3PModelCapabilityOverride(model,input.at("capability").get<std::string>(),s);
    result=x?Json(*x):Json(nullptr);
  }
  else if(op=="capability"){
    auto x=getModelCapability(model,s);result=x?*x:Json(nullptr);
  }
  else if(op=="supports1m")result=modelSupports1M(model,s);
  else if(op=="treatment")result=getStandardOneMillionTreatmentEnabled(model,s);
  else throw std::logic_error("unknown op");
  return {{"result",result},{"logs",logs}};
}
int main(){std::string line;while(std::getline(std::cin,line))try{std::cout<<evaluate(Json::parse(line)).dump()<<'\n';}
 catch(const std::exception& e){std::cout<<Json{{"fatal",e.what()}}.dump()<<'\n';}}
