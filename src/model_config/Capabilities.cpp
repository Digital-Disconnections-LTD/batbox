#include "model_config/Internal.hpp"

namespace batbox::model_config {
bool modelSupportsEffort(const std::string& model,Services& services){
  const auto m=internal::lower(model);
  if(internal::envTruthy(services,"BATBOX_ALWAYS_ENABLE_EFFORT"))return true;
  const auto supported=get3PModelCapabilityOverride(model,"effort",services);
  if(supported.has_value())return *supported;
  if(internal::contains(m,internal::pattern(services,"premiumCurrentFamily"))||
      internal::contains(m,internal::pattern(services,"standardCurrentFamily")))return true;
  if(internal::contains(m,internal::pattern(services,"fastFamily"))||
      internal::contains(m,internal::pattern(services,"standardFamily"))||
      internal::contains(m,internal::pattern(services,"premiumFamily")))return false;
  return internal::provider(services)=="firstParty";
}
bool modelSupportsStructuredOutputs(const std::string& model,Services& services){
  const auto canonical=getCanonicalName(model,services);
  const auto provider=internal::provider(services);
  if(provider!="firstParty"&&provider!="foundry")return false;
  return internal::contains(canonical,internal::pattern(services,"standardCurrentFull"))||
    internal::contains(canonical,internal::pattern(services,"standardPreviousFull"))||
    internal::contains(canonical,internal::pattern(services,"premiumLegacy41Full"))||
    internal::contains(canonical,internal::pattern(services,"premiumPreviousFull"))||
    internal::contains(canonical,internal::pattern(services,"premiumCurrentFull"))||
    internal::contains(canonical,internal::pattern(services,"fastCurrentFull"));
}
bool modelSupportsThinking(const std::string& model,Services& services){
  const auto supported=get3PModelCapabilityOverride(model,"thinking",services);
  if(supported.has_value())return *supported;
  if(internal::internalUser(services)){
    internal::require(services.resolveInternalModel,"resolveInternalModel");
    if(services.resolveInternalModel(internal::lower(model)))return true;
  }
  const auto canonical=getCanonicalName(model,services);
  const auto provider=internal::provider(services);
  if(provider=="foundry"||provider=="firstParty")
    return !internal::contains(canonical,internal::pattern(services,"legacy3Prefix"));
  return internal::contains(canonical,internal::pattern(services,"standard4Family"))||
    internal::contains(canonical,internal::pattern(services,"premium4Family"));
}
bool modelSupportsAdaptiveThinking(const std::string& model,Services& services){
  const auto supported=get3PModelCapabilityOverride(model,"adaptive_thinking",services);
  if(supported.has_value())return *supported;
  const auto canonical=getCanonicalName(model,services);
  if(internal::contains(canonical,internal::pattern(services,"premiumCurrentFamily"))||
      internal::contains(canonical,internal::pattern(services,"standardCurrentFamily")))return true;
  if(internal::contains(canonical,internal::pattern(services,"premiumFamily"))||
      internal::contains(canonical,internal::pattern(services,"standardFamily"))||
      internal::contains(canonical,internal::pattern(services,"fastFamily")))return false;
  const auto provider=internal::provider(services);
  return provider=="firstParty"||provider=="foundry";
}
bool isFastModeSupportedByModel(const std::optional<std::string>& modelSetting,Services& services){
  if(!services.fastModeEnabled)throw std::logic_error("fastModeEnabled dependency not supplied");
  if(!*services.fastModeEnabled)return false;
  const auto model=modelSetting.value_or(services.defaultMainLoopModelSetting);
  if(model.empty())throw std::logic_error("defaultMainLoopModelSetting dependency not supplied");
  internal::require(services.parseUserSpecifiedModel,"parseUserSpecifiedModel");
  const auto parsed=services.parseUserSpecifiedModel(model);
  return internal::contains(internal::lower(parsed),internal::pattern(services,"premiumCurrentFamily"));
}
} // namespace batbox::model_config
