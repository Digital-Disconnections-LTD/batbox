#include "model_config/Internal.hpp"
#include <regex>

namespace batbox::model_config {
std::string resolveOverriddenModel(const std::string& modelId,const Services& services){
  if(!services.modelOverrides.is_object())return modelId;
  for(auto it=services.modelOverrides.begin();it!=services.modelOverrides.end();++it)
    if(it.value().is_string()&&it.value().get<std::string>()==modelId)return it.key();
  return modelId;
}
std::string firstPartyNameToCanonical(const std::string& rawName,const Services& services){
  std::string name=internal::lower(rawName);
  for(const auto& key:{"premiumCurrentFull","premiumPreviousFull","premiumLegacy41Full","premiumLegacy4Full",
      "standardCurrentFull","standardPreviousFull","standardLegacy4Full","fastCurrentFull",
      "legacy37StandardFull","legacy35StandardFull","legacy35FastFull","legacy3PremiumFull",
      "legacy3StandardFull","legacy3FastFull"}){
    const auto value=internal::pattern(services,key);
    if(internal::contains(name,value))return value;
  }
  std::string prefix=internal::pattern(services,"vendorPrefix");
  static const std::regex special(R"([.^$|()\[\]{}*+?\\])");
  prefix=std::regex_replace(prefix,special,R"(\$&)");
  const std::regex fallback("("+prefix+R"((\d+-\d+-)?\w+))");
  std::smatch match;
  if(std::regex_search(name,match,fallback)&&match.size()>1&&!match[1].str().empty())return match[1].str();
  return name;
}
std::string getCanonicalName(const std::string& model,const Services& services){
  return firstPartyNameToCanonical(resolveOverriddenModel(model,services),services);
}
std::string normalizeModelStringForAPI(const std::string& model){
  static const std::regex suffix(R"(\[(1|2)m\])",std::regex::icase);
  return std::regex_replace(model,suffix,"");
}
std::optional<bool> get3PModelCapabilityOverride(const std::string& model,const std::string& capability,Services& services){
  const auto key=std::make_pair(internal::lower(model),capability);
  const auto existing=services.capabilityOverrideMemo.find(key);
  if(existing!=services.capabilityOverrideMemo.end())return existing->second;
  std::optional<bool> result;
  if(internal::provider(services)=="firstParty"){
    services.capabilityOverrideMemo[key]=std::nullopt;return std::nullopt;
  }
  if(!services.pinnedCapabilityTiersConfigured)throw std::logic_error("pinnedCapabilityTiers dependency not supplied");
  for(const auto& tier:services.pinnedCapabilityTiers){
    const std::string pinned=internal::envString(services,internal::field(tier,"modelEnvVar"));
    const Json capabilities=internal::envValue(services,internal::field(tier,"capabilitiesEnvVar"));
    if(pinned.empty()||capabilities.is_null())continue;
    if(internal::lower(model)!=internal::lower(pinned))continue;
    const std::string list=internal::lower(internal::str(capabilities));
    std::size_t start=0;
    while(start<=list.size()){
      const auto end=list.find(',',start);
      const auto item=value::trim(list.substr(start,end==std::string::npos?std::string::npos:end-start));
      if(item==capability){result=true;break;}
      if(end==std::string::npos)break;
      start=end+1;
    }
    if(!result)result=false;
    break;
  }
  services.capabilityOverrideMemo[key]=result;
  return result;
}
} // namespace batbox::model_config
