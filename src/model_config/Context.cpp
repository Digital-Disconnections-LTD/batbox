#include "model_config/Internal.hpp"
#include <algorithm>
#include <regex>

namespace batbox::model_config {
namespace {
constexpr double kContextDefault=200000;
}

std::optional<Json> getModelCapability(const std::string& model,Services& services){
  if(!internal::internalUser(services))return std::nullopt;
  if(internal::provider(services)!="firstParty")return std::nullopt;
  if(!services.firstPartyBaseUrl)return std::nullopt;
  if(!services.capabilityStashLoaded)throw std::logic_error("capabilityCache dependency not supplied");
  if(!services.capabilityStash.is_array()||services.capabilityStash.empty())return std::nullopt;
  const auto m=internal::lower(model);
  for(const auto& cap:services.capabilityStash)
    if(internal::lower(internal::field(cap,"id"))==m)return cap;
  for(const auto& cap:services.capabilityStash)
    if(internal::contains(m,internal::lower(internal::field(cap,"id"))))return cap;
  return std::nullopt;
}

bool is1mContextDisabled(const Services& services){
  return internal::envTruthy(services,"BATBOX_DISABLE_1M_CONTEXT");
}
bool has1mContext(const std::string& model,const Services& services){
  if(is1mContextDisabled(services))return false;
  static const std::regex suffix(R"(\[1m\])",std::regex::icase);
  return std::regex_search(model,suffix);
}
bool modelSupports1M(const std::string& model,const Services& services){
  if(is1mContextDisabled(services))return false;
  const auto canonical=getCanonicalName(model,services);
  return internal::contains(canonical,internal::pattern(services,"standard4Family"))||
    internal::contains(canonical,internal::pattern(services,"premiumCurrentFamily"));
}
bool getStandardOneMillionTreatmentEnabled(const std::string& model,const Services& services){
  if(is1mContextDisabled(services))return false;
  if(has1mContext(model,services))return false;
  if(!internal::contains(getCanonicalName(model,services),internal::pattern(services,"standardCurrentFamily")))return false;
  return internal::field(services.localConfig,"standardOneMillionTreatment")=="true";
}
double getContextWindowForModel(const std::string& model,const Json& betas,Services& services){
  if(internal::internalUser(services)&&!internal::envString(services,"BATBOX_MAX_CONTEXT_TOKENS").empty()){
    const double override=value::parseInt10(internal::envString(services,"BATBOX_MAX_CONTEXT_TOKENS"));
    if(!std::isnan(override)&&override>0)return override;
  }
  if(has1mContext(model,services))return 1000000;
  const auto cap=getModelCapability(model,services);
  if(cap){
    const auto maxInput=internal::get(*cap,"max_input_tokens");
    if(value::truthy(maxInput)&&maxInput.is_number()&&maxInput.get<double>()>=100000){
      if(maxInput.get<double>()>kContextDefault&&is1mContextDisabled(services))return kContextDefault;
      return maxInput.get<double>();
    }
  }
  if(betas.is_array()){
    const auto beta=internal::pattern(services,"contextOneMillionBetaHeader");
    for(const auto& item:betas)
      if(item.is_string()&&item.get<std::string>()==beta&&modelSupports1M(model,services))return 1000000;
  }
  if(getStandardOneMillionTreatmentEnabled(model,services))return 1000000;
  if(internal::internalUser(services)){
    internal::require(services.resolveInternalModel,"resolveInternalModel");
    auto internalModel=services.resolveInternalModel(model);
    if(internalModel&&value::truthy(internal::get(*internalModel,"contextWindow")))
      return value::numberOr(internal::get(*internalModel,"contextWindow"));
  }
  return kContextDefault;
}
Json calculateContextPercentages(const Json& currentUsage,double contextWindowSize){
  if(currentUsage.is_null())return {{"used",nullptr},{"remaining",nullptr}};
  const double total=value::numberOr(internal::get(currentUsage,"input_tokens"))+
    value::numberOr(internal::get(currentUsage,"cache_creation_input_tokens"))+
    value::numberOr(internal::get(currentUsage,"cache_read_input_tokens"));
  const double used=value::round((total/contextWindowSize)*100);
  const double clamped=std::min(100.0,std::max(0.0,used));
  return {{"used",clamped},{"remaining",100.0-clamped}};
}
} // namespace batbox::model_config
