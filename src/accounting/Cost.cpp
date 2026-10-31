#include "accounting/Accounting.hpp"
#include <stdexcept>

namespace batbox::accounting {
namespace {
template<class F>void need(const F& callback,const char* name){
  if(!callback)throw std::logic_error(std::string(name)+" dependency not supplied");
}
double number(const Json& value,const char* name){
  if(!value.is_number())throw std::invalid_argument(std::string(name)+" must be a number");
  return value.get<double>();
}
double required(const Json& value,const char* key){
  if(!value.is_object()||!value.contains(key))throw std::invalid_argument(std::string(key)+" required");
  return number(value.at(key),key);
}
double optional(const Json& value,const char* key){
  if(!value.is_object()||!value.contains(key)||value.at(key).is_null())return 0;
  return number(value.at(key),key);
}
}
Json getPremiumCurrentCostTier(bool fastMode,const CostServices& services){
  need(services.isFastModeEnabled,"isFastModeEnabled");
  if(services.isFastModeEnabled()&&fastMode){
    if(!services.premiumCurrentFastCosts.is_object())throw std::logic_error("premiumCurrentFastCosts dependency not supplied");
    return services.premiumCurrentFastCosts;
  }
  if(!services.premiumCurrentStandardCosts.is_object())throw std::logic_error("premiumCurrentStandardCosts dependency not supplied");
  return services.premiumCurrentStandardCosts;
}
Json getModelCosts(const std::string& model,const Json& usage,
  AccountingState& state,const CostServices& services){
  need(services.getCanonicalName,"getCanonicalName");
  const auto shortName=services.getCanonicalName(model);
  if(services.premiumCurrentCanonical.empty())throw std::logic_error("premiumCurrentCanonical dependency not supplied");
  if(shortName==services.premiumCurrentCanonical){
    const bool fast=usage.is_object()&&usage.value("speed",std::string())=="fast";
    return getPremiumCurrentCostTier(fast,services);
  }
  if(services.modelCosts.is_object()&&services.modelCosts.contains(shortName))
    return services.modelCosts.at(shortName);
  state.hasUnknownModelCost=true;
  need(services.getDefaultMainLoopModelSetting,"getDefaultMainLoopModelSetting");
  const auto defaultShortName=services.getCanonicalName(services.getDefaultMainLoopModelSetting());
  if(services.modelCosts.is_object()&&services.modelCosts.contains(defaultShortName))
    return services.modelCosts.at(defaultShortName);
  if(!services.unknownFallbackCosts.is_object())throw std::logic_error("unknownFallbackCosts dependency not supplied");
  return services.unknownFallbackCosts;
}
double tokensToUSDCost(const Json& costs,const Json& usage){
  return (required(usage,"input_tokens")/1000000)*required(costs,"inputTokens")+
    (required(usage,"output_tokens")/1000000)*required(costs,"outputTokens")+
    (optional(usage,"cache_read_input_tokens")/1000000)*required(costs,"promptCacheReadTokens")+
    (optional(usage,"cache_creation_input_tokens")/1000000)*required(costs,"promptCacheWriteTokens")+
    optional(usage.is_object()&&usage.contains("server_tool_use")?usage.at("server_tool_use"):Json(),"web_search_requests")*
      required(costs,"webSearchRequests");
}
double calculateUSDCost(const std::string& resolvedModel,const Json& usage,
  AccountingState& state,const CostServices& services){
  const auto costs=getModelCosts(resolvedModel,usage,state,services);
  return tokensToUSDCost(costs,usage);
}
double calculateCostFromTokens(const std::string& model,const Json& tokens,
  AccountingState& state,const CostServices& services){
  Json usage={{"input_tokens",tokens.at("inputTokens")},{"output_tokens",tokens.at("outputTokens")},
    {"cache_read_input_tokens",tokens.at("cacheReadInputTokens")},
    {"cache_creation_input_tokens",tokens.at("cacheCreationInputTokens")}};
  return calculateUSDCost(model,usage,state,services);
}
} // namespace batbox::accounting
