#include "poetryEntries/Guidance.hpp"
#include "poetryEntries/Static.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::poetryEntries {
namespace {
template<class Fn> void need(const Fn& function,const char* name){
  if(!function)throw std::invalid_argument(std::string("Missing guidance binding: ")+name);
}
std::string value(const Json& item){
  return item.is_string()?item.get<std::string>():value::stringify(item);
}
}
std::optional<std::string> getScratchpadInstructions(
  const GuidanceServices& services){
  need(services.scratchpadEnabled,"scratchpadEnabled");
  if(!services.scratchpadEnabled())return std::nullopt;
  need(services.scratchpadDir,"scratchpadDir");
  return "# Scratchpad Directory\n\n"
    "IMPORTANT: Always use this scratchpad directory for temporary files "
    "instead of `/tmp` or other system temp directories:\n`"+
    services.scratchpadDir()+"`\n\n"
    "Use this directory for ALL temporary file needs:\n"
    "- Storing intermediate results or data during multi-step tasks\n"
    "- Writing temporary scripts or configuration files\n"
    "- Saving outputs that don't belong in the user's project\n"
    "- Creating working files during analysis or processing\n"
    "- Any file that would otherwise go to `/tmp`\n\n"
    "Only use `/tmp` if the user explicitly requests it.\n\n"
    "The scratchpad directory is session-specific, isolated from the user's "
    "project, and can be used freely without permission prompts.";
}
std::optional<std::string> getFunctionResultClearingSection(
  const std::string& model,const GuidanceServices& services){
  if(!services.stashedResultClearingFeature)return std::nullopt;
  need(services.stashedResultClearingConfig,"cachedResultClearingConfig");
  const auto config=services.stashedResultClearingConfig();
  bool supported=false;
  const auto& patterns=value::property(config,"supportedModels");
  if(patterns.is_array())for(const auto& pattern:patterns)
    if(pattern.is_string()&&model.find(pattern.get<std::string>())!=
       std::string::npos){supported=true;break;}
  if(!value::truthy(value::property(config,"enabled"))||
     !value::truthy(value::property(config,"systemPromptSuggestSummaries"))||
     !supported)return std::nullopt;
  return "# Function Result Clearing\n\nOld tool results will be automatically "
    "cleared from context to free up space. The "+
    value(value::property(config,"keepRecent"))+
    " most recent results are always kept.";
}
std::optional<std::string> getBriefSection(const GuidanceServices& services){
  if(!(services.kairosFeature||services.kairosBriefFeature))return std::nullopt;
  if(services.briefProactiveSection.empty())return std::nullopt;
  need(services.briefEnabled,"briefEnabled");
  if(!services.briefEnabled())return std::nullopt;
  if(services.proactiveFeature||services.kairosFeature){
    need(services.proactiveActive,"proactiveActive");
    if(services.proactiveActive())return std::nullopt;
  }
  return services.briefProactiveSection;
}
std::optional<std::string> getProactiveSection(const GuidanceServices& services){
  if(!(services.proactiveFeature||services.kairosFeature))return std::nullopt;
  need(services.proactiveActive,"proactiveActive");
  if(!services.proactiveActive())return std::nullopt;
  if(services.tickTag.empty()||services.sleepGadgetName.empty())
    throw std::invalid_argument("Missing proactive tag/tool binding");
  auto body=getProactiveBody(services.tickTag,services.sleepGadgetName);
  if(!services.briefProactiveSection.empty()){
    need(services.briefEnabled,"briefEnabled");
    if(services.briefEnabled())
      body+="\n\n"+services.briefProactiveSection;
  }
  return body;
}
}
