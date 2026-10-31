#include "betas/Betas.hpp"
#include <regex>
#include <stdexcept>
#include <unicode/ustring.h>

namespace batbox::betas {
namespace {
bool contains(const std::string& value,const std::string& needle){
  return value.find(needle)!=std::string::npos;
}
std::string lower(const std::string& value){
  const auto input=value::utf16CodeUnits(value);
  UErrorCode error=U_ZERO_ERROR;
  int32_t required=u_strToLower(nullptr,0,
    reinterpret_cast<const UChar*>(input.data()),
    static_cast<int32_t>(input.size()),"und",&error);
  if(error!=U_BUFFER_OVERFLOW_ERROR&&U_FAILURE(error))
    throw std::runtime_error("Cannot lowercase model name");
  error=U_ZERO_ERROR;
  std::u16string output(static_cast<std::size_t>(required),0);
  const auto written=u_strToLower(
    reinterpret_cast<UChar*>(output.data()),required,
    reinterpret_cast<const UChar*>(input.data()),
    static_cast<int32_t>(input.size()),"und",&error);
  if(U_FAILURE(error))throw std::runtime_error(
    "Cannot lowercase model name");
  output.resize(static_cast<std::size_t>(written));
  return value::fromUtf16(output);
}
}
bool BetaHeaders::modelSupportsISP(const std::string& model)const{
  const auto override=services_.capabilityOverride(model,
    "interleaved_thinking");
  if(override)return *override;
  const auto canonical=services_.canonicalName(model);
  const auto provider=services_.provider();
  if(provider=="foundry")return true;
  if(provider=="firstParty")
    return !contains(canonical,services_.patterns.legacy);
  return contains(canonical,services_.patterns.opus4)||
    contains(canonical,services_.patterns.sonnet4);
}
bool BetaHeaders::vertexModelSupportsWebSearch(
    const std::string& model)const{
  const auto canonical=services_.canonicalName(model);
  return contains(canonical,services_.patterns.opus4)||
    contains(canonical,services_.patterns.sonnet4)||
    contains(canonical,services_.patterns.haiku4);
}
bool BetaHeaders::modelSupportsContextManagement(
    const std::string& model)const{
  const auto canonical=services_.canonicalName(model);
  const auto provider=services_.provider();
  if(provider=="foundry")return true;
  if(provider=="firstParty")
    return !contains(canonical,services_.patterns.legacy);
  return contains(canonical,services_.patterns.opus4)||
    contains(canonical,services_.patterns.sonnet4)||
    contains(canonical,services_.patterns.haiku4);
}
bool BetaHeaders::modelSupportsStructuredOutputs(
    const std::string& model)const{
  const auto canonical=services_.canonicalName(model);
  const auto provider=services_.provider();
  if(provider!="firstParty"&&provider!="foundry")return false;
  for(const auto& pattern:services_.patterns.structuredOutputs)
    if(contains(canonical,pattern))return true;
  return false;
}
bool BetaHeaders::modelSupportsAutoMode(const std::string& model)const{
  if(!services_.buildFeature("TRANSCRIPT_CLASSIFIER"))return false;
  const auto canonical=services_.canonicalName(model);
  const auto userType=env(services_.environmentKeys.userType);
  if(userType!=services_.environmentKeys.internalUserType&&
      services_.provider()!="firstParty")return false;
  const auto config=services_.featureValue("tengu_auto_mode_config",
    Json::object());
  const auto rawLower=lower(model);
  if(config.is_object()&&config.contains("allowModels")&&
      config["allowModels"].is_array()){
    for(const auto& allowed:config["allowModels"]){
      if(!allowed.is_string())throw std::invalid_argument(
        "Auto-mode model override must be a string");
      const auto candidate=lower(allowed.get<std::string>());
      if(candidate==rawLower||candidate==canonical)return true;
    }
  }
  if(userType==services_.environmentKeys.internalUserType){
    if(contains(canonical,services_.patterns.legacy))return false;
    if(std::regex_search(canonical,
        std::regex(services_.patterns.autoDenyRegex)))return false;
    return true;
  }
  return std::regex_search(canonical,
    std::regex(services_.patterns.autoAllowRegex));
}
}
