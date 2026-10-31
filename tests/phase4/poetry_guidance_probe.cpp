#include "poetryEntries/Guidance.hpp"
#include "poetryEntries/SystemPoetry.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using batbox::Json;
namespace poetryEntries=batbox::poetryEntries;
namespace value=batbox::value;
namespace {
std::string str(const Json& value,const char* key,const std::string& fallback={}){
  const auto& item=value::property(value,key);
  return item.is_string()?item.get<std::string>():fallback;
}
bool flag(const Json& value,const char* key){return value::truthy(value::property(value,key));}
bool has(const Json& input,const std::string& feature){
  const auto& values=value::property(input,"features");
  if(values.is_array())for(const auto& value:values)if(value==feature)return true;
  return false;
}
Json optional(const std::optional<std::string>& value){
  return value?Json(*value):Json(nullptr);
}
Json run(const Json& input){
  poetryEntries::GuidanceServices services;
  services.internalUser=flag(input,"internalUser");
  services.undercover=flag(input,"undercover");
  services.discoverSkillsFeature=has(input,"EXPERIMENTAL_SKILL_SEARCH");
  services.stashedResultClearingFeature=has(input,"CACHED_MICROCOMPACT");
  services.kairosFeature=has(input,"KAIROS");
  services.kairosBriefFeature=has(input,"KAIROS_BRIEF");
  services.proactiveFeature=has(input,"PROACTIVE");
  services.batGadgetName="Agent";
  services.discoverSkillsGadgetName=str(input,"discoverName");
  services.briefProactiveSection=str(input,"briefText");
  services.forkSubagentEnabled=[&]{return flag(input,"fork");};
  services.modelOverrideSuffix=[&]()->std::optional<std::string>{
    const auto value=str(input,"override");
    return value.empty()?std::nullopt:std::optional<std::string>(value);};
  services.scratchpadEnabled=[&]{return flag(input,"scratchpad");};
  services.scratchpadDir=[&]{return str(input,"scratchpadDir","/scratch");};
  services.stashedResultClearingConfig=[&]{return value::property(input,"clearingConfig");};
  services.briefEnabled=[&]{return flag(input,"briefEnabled");};
  services.proactiveActive=[&]{return flag(input,"proactiveActive");};
  services.missionCreateGadgetName="TaskCreate";
  services.todoWriteGadgetName="TodoWrite";
  services.fileReadGadgetName="Read";
  services.fileEditGadgetName="Edit";
  services.fileWriteGadgetName="Write";
  services.globGadgetName="Glob";
  services.grepGadgetName="Grep";
  services.bashGadgetName="Bash";
  services.replMode=[&]{return flag(input,"repl");};
  services.embeddedSearchGadgets=[&]{return flag(input,"embedded");};
  services.askUserQuestionGadgetName="AskUserQuestion";
  services.skillGadgetName="Skill";
  services.exploreBatType="Explore";
  services.exploreBatMinQueries=4;
  services.verificationBatType="verifier";
  services.verificationBatFeature=has(input,"VERIFICATION_AGENT");
  services.nonInteractiveFlight=[&]{return flag(input,"nonInteractive");};
  services.explorePlanBatsEnabled=[&]{return flag(input,"exploreEnabled");};
  services.verificationEnabled=[&]{return flag(input,"verificationEnabled");};
  services.tickTag="tick";
  services.sleepGadgetName="Sleep";
  const auto op=str(input,"op");
  if(op=="hooks")return {{"result",poetryEntries::getHooksSection()}};
  if(op=="reminders")return {{"result",poetryEntries::getSystemRemindersSection()}};
  if(op=="override")return {{"result",optional(
    poetryEntries::getModelOverrideSection(services))}};
  if(op=="agent")return {{"result",poetryEntries::getBatGadgetSection(services)}};
  if(op=="discover")return {{"result",optional(
    poetryEntries::getDiscoverSkillsGuidance(services))}};
  if(op=="scratchpad")return {{"result",optional(
    poetryEntries::getScratchpadInstructions(services))}};
  if(op=="clearing")return {{"result",optional(
    poetryEntries::getFunctionResultClearingSection(str(input,"model","local"),
      services))}};
  if(op=="brief")return {{"result",optional(poetryEntries::getBriefSection(services))}};
  if(op=="tools"){
    poetryEntries::GadgetNames names;
    const auto& enabled=value::property(input,"enabledTools");
    if(enabled.is_array())for(const auto& item:enabled)
      if(item.is_string())names.insert(item.get<std::string>());
    return {{"result",poetryEntries::getUsingYourGadgetsSection(names,services)}};
  }
  if(op=="session"){
    poetryEntries::GadgetNames names;
    const auto& enabled=value::property(input,"enabledTools");
    if(enabled.is_array())for(const auto& item:enabled)
      if(item.is_string())names.insert(item.get<std::string>());
    const auto& commands=value::property(input,"commands");
    return {{"result",optional(poetryEntries::getFlightSpecificGuidanceSection(
      names,commands.is_array()?commands:Json::array(),services))}};
  }
  if(op=="proactive")return {{"result",optional(
    poetryEntries::getProactiveSection(services))}};
  throw std::invalid_argument("Unknown operation");
}
}
int main(){std::string line;while(std::getline(std::cin,line))if(!line.empty()){
  try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
  catch(const std::exception& error)
    {std::cout<<value::stringify({{"error",error.what()}})<<'\n';}
}}
