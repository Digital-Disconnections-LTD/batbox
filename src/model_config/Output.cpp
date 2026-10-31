#include "model_config/Internal.hpp"
#include <algorithm>
#include <cmath>

namespace batbox::model_config {
Json getModelMaxOutputTokens(const std::string& model,Services& services){
  double defaultTokens;
  double upperLimit;
  if(internal::internalUser(services)){
    internal::require(services.resolveInternalModel,"resolveInternalModel");
    const auto internalModel=services.resolveInternalModel(internal::lower(model));
    if(internalModel){
      const auto configuredDefault=internal::get(*internalModel,"defaultMaxTokens");
      const auto configuredUpper=internal::get(*internalModel,"upperMaxTokensLimit");
      defaultTokens=configuredDefault.is_null()?32000:value::numberOr(configuredDefault);
      upperLimit=configuredUpper.is_null()?64000:value::numberOr(configuredUpper);
      return {{"default",defaultTokens},{"upperLimit",upperLimit}};
    }
  }
  const auto m=getCanonicalName(model,services);
  if(internal::contains(m,internal::pattern(services,"premiumCurrentFamily"))){defaultTokens=64000;upperLimit=128000;}
  else if(internal::contains(m,internal::pattern(services,"standardCurrentFamily"))){defaultTokens=32000;upperLimit=128000;}
  else if(internal::contains(m,internal::pattern(services,"premiumPreviousFamily"))||
      internal::contains(m,internal::pattern(services,"standard4Family"))||
      internal::contains(m,internal::pattern(services,"fast4Family"))){defaultTokens=32000;upperLimit=64000;}
  else if(internal::contains(m,internal::pattern(services,"premiumLegacy41Family"))||
      internal::contains(m,internal::pattern(services,"premiumLegacy4Family"))){defaultTokens=32000;upperLimit=32000;}
  else if(internal::contains(m,internal::pattern(services,"legacy3PremiumFamily"))){defaultTokens=4096;upperLimit=4096;}
  else if(internal::contains(m,internal::pattern(services,"legacy3StandardFamily"))){defaultTokens=8192;upperLimit=8192;}
  else if(internal::contains(m,internal::pattern(services,"legacy3FastFamily"))){defaultTokens=4096;upperLimit=4096;}
  else if(internal::contains(m,internal::pattern(services,"legacy35StandardFamily"))||
      internal::contains(m,internal::pattern(services,"legacy35FastFamily"))){defaultTokens=8192;upperLimit=8192;}
  else if(internal::contains(m,internal::pattern(services,"legacy37StandardFamily"))){defaultTokens=32000;upperLimit=64000;}
  else {defaultTokens=32000;upperLimit=64000;}
  const auto cap=getModelCapability(model,services);
  if(cap){
    const auto maxTokens=internal::get(*cap,"max_tokens");
    if(value::truthy(maxTokens)&&maxTokens.is_number()&&maxTokens.get<double>()>=4096){
      upperLimit=maxTokens.get<double>();
      defaultTokens=std::min(defaultTokens,upperLimit);
    }
  }
  return {{"default",defaultTokens},{"upperLimit",upperLimit}};
}

double getMaxThinkingTokensForModel(const std::string& model,Services& services){
  return getModelMaxOutputTokens(model,services).at("upperLimit").get<double>()-1;
}
Json validateBoundedIntEnvVar(const std::string& name,const std::string& value,
  double defaultValue,double upperLimit,Services& services){
  if(value.empty())return {{"effective",defaultValue},{"status","valid"}};
  const double parsed=value::parseInt10(value);
  if(std::isnan(parsed)||parsed<=0){
    const auto message="Invalid value \""+value+"\" (using default: "+value::stringify(defaultValue)+")";
    internal::require(services.logForDebugging,"logForDebugging");
    services.logForDebugging(name+" "+message);
    return {{"effective",defaultValue},{"status","invalid"},{"message",message}};
  }
  if(parsed>upperLimit){
    const auto message="Capped from "+value::stringify(parsed)+" to "+value::stringify(upperLimit);
    internal::require(services.logForDebugging,"logForDebugging");
    services.logForDebugging(name+" "+message);
    return {{"effective",upperLimit},{"status","capped"},{"message",message}};
  }
  return {{"effective",parsed},{"status","valid"}};
}

double getMaxOutputTokensForModel(const std::string& model,Services& services){
  const auto max=getModelMaxOutputTokens(model,services);
  if(!services.slotReservationCapEnabled)throw std::logic_error("slotReservationCapEnabled dependency not supplied");
  const auto defaultTokens=*services.slotReservationCapEnabled?
    std::min(max.at("default").get<double>(),8000.0):max.at("default").get<double>();
  const auto result=validateBoundedIntEnvVar("BATBOX_MAX_OUTPUT_TOKENS",
    internal::envString(services,"BATBOX_MAX_OUTPUT_TOKENS"),defaultTokens,
    max.at("upperLimit").get<double>(),services);
  return result.at("effective").get<double>();
}
} // namespace batbox::model_config
