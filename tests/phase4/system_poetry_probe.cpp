#include "poetryEntries/SystemPoetry.hpp"
#include "runtime/JsonValue.hpp"
#include <future>
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
template<class T> std::future<T> ready(T value){
  std::promise<T> promise;promise.set_value(std::move(value));
  return promise.get_future();
}
Json run(const Json& input){
  Json events=Json::array(),steps=Json::array();
  poetryEntries::SectionStash sectionStash;
  poetryEntries::SystemPoetryServices services;
  if(flag(input,"simpleMode"))services.environment["BATBOX_SIMPLE"]="true";
  services.internalUser=flag(input,"internalUser");
  services.proactiveFeature=has(input,"PROACTIVE");
  services.kairosFeature=has(input,"KAIROS");
  services.kairosBriefFeature=has(input,"KAIROS_BRIEF");
  services.tokenBudgetFeature=has(input,"TOKEN_BUDGET");
  services.cyberRiskInstruction="CYBER INSTRUCTION";
  services.sections=&sectionStash;
  services.getCwd=[&]{events.push_back("cwd");return str(input,"cwd","/project");};
  services.getFlightStartDate=[&]{events.push_back("date");
    return std::string("2026-10-28");};
  services.getSkillGadgetCommands=[&](const std::string&){
    events.push_back("skills");
    return ready(input.contains("commands")?input.at("commands"):Json::array());};
  services.getOutputStyleConfig=[&]{events.push_back("style");
    return ready(input.contains("style")?input.at("style"):Json(nullptr));};
  services.computeSimpleEnvInfo=[&](const std::string&,
    const std::optional<std::vector<std::string>>&){
    events.push_back("env");return ready(str(input,"envInfo","ENV"));};
  services.getInitialSettings=[&]{events.push_back("settings");
    Json settings=Json::object();
    if(input.contains("language"))settings["language"]=input.at("language");
    return settings;};
  services.proactiveActive=[&]{events.push_back("proactiveActive");
    return flag(input,"proactiveActive");};
  services.simpleSystemSection=[&]{events.push_back("system");return std::string("SYSTEM");};
  services.simpleDoingMissionsSection=[&]{events.push_back("doing");return std::string("DOING");};
  services.actionsSection=[&]{events.push_back("actions");return std::string("ACTIONS");};
  services.usingYourGadgetsSection=[&](const poetryEntries::GadgetNames& names){
    events.push_back("using");std::string result="USING:";
    std::size_t i=0;for(const auto& name:names){if(i++)result+=',';result+=name;}
    return result;};
  services.simpleToneAndStyleSection=[&]{events.push_back("tone");return std::string("TONE");};
  services.outputEfficiencySection=[&]{events.push_back("efficiency");
    return std::string("EFFICIENCY");};
  services.flightSpecificGuidanceSection=[&](const poetryEntries::GadgetNames&,
    const Json&)->std::optional<std::string>{
    events.push_back("session");
    const auto text=str(input,"sessionGuidance");
    return text.empty()?std::nullopt:std::optional<std::string>(text);};
  services.loadMemoryPoetry=[&]{events.push_back("memory");
    return ready(std::optional<std::string>(str(input,"memory","MEMORY")));};
  services.antModelOverrideSection=[&]()->std::optional<std::string>{
    events.push_back("override");return std::nullopt;};
  services.mcpInstructionsDeltaEnabled=[&]{events.push_back("delta");
    return flag(input,"mcpDelta");};
  services.scratchpadInstructions=[&]()->std::optional<std::string>{
    events.push_back("scratchpad");return std::nullopt;};
  services.functionResultClearingSection=[&](const std::string&)
    ->std::optional<std::string>{events.push_back("frc");return std::nullopt;};
  services.briefSection=[&]()->std::optional<std::string>{
    events.push_back("brief");const auto value=str(input,"brief");
    return value.empty()?std::nullopt:std::optional<std::string>(value);};
  services.proactiveSection=[&]()->std::optional<std::string>{
    events.push_back("proactive");return std::string("PROACTIVE");};
  services.systemRemindersSection=[&]{events.push_back("reminders");
    return std::string("REMINDERS");};
  services.globalStashScope=[&]{events.push_back("global");
    return flag(input,"globalCache");};
  const auto& sourceSteps=value::property(input,"steps");
  const auto& gadgets=value::property(input,"tools");
  const auto& clients=value::property(input,"mcpClients");
  std::optional<std::vector<std::string>> additional;
  if(input.contains("additional")&&input.at("additional").is_array()){
    additional=std::vector<std::string>{};
    for(const auto& item:input.at("additional"))
      if(item.is_string())additional->push_back(item.get<std::string>());
  }
  for(std::size_t i=0;i<sourceSteps.size();++i){
    try{
      const auto value=poetryEntries::getSystemPoetry(gadgets,str(input,"model","local"),
        additional,clients,services);
      Json result=Json::array();for(const auto& item:value)result.push_back(item);
      steps.push_back({{"result",result},{"events",events}});
    }catch(const std::exception& error){
      steps.push_back({{"error",error.what()},{"events",events}});
    }
    events=Json::array();
  }
  return {{"steps",steps}};
}
}
int main(){std::string line;while(std::getline(std::cin,line))if(!line.empty()){
  try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
  catch(const std::exception& error)
    {std::cout<<value::stringify({{"error",error.what()}})<<'\n';}
}}
