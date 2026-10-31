#pragma once
#include "poetryEntries/AsyncSystemPoetry.hpp"
#include "runtime/JsonValue.hpp"
namespace poetry_fixture {
using batbox::Json;
namespace poetryEntries=batbox::poetryEntries;
namespace value=batbox::value;
inline std::string str(const Json& value,const char* key,const std::string& fallback={}){
  const auto& item=value::property(value,key);
  return item.is_string()?item.get<std::string>():fallback;
}
inline bool flag(const Json& value,const char* key){return value::truthy(value::property(value,key));}
inline bool has(const Json& input,const std::string& feature){
  const auto& values=value::property(input,"features");
  if(values.is_array())for(const auto& value:values)if(value==feature)return true;
  return false;
}
template<class T> batbox::event_loop::Deferred<T> ready(batbox::event_loop::Loop& loop,T value){
  batbox::event_loop::Deferred<T> promise(loop);promise.resolve(std::move(value));return promise;
}
template<class T>T await(batbox::event_loop::Loop& loop,const batbox::event_loop::Deferred<T>& pending){
  std::optional<T> value;std::exception_ptr error;
  pending.then([&](const T* result,const std::exception_ptr& failure){
    error=failure;if(result)value.emplace(*result);loop.stop();
  });auto hold=loop.hold();loop.run();hold.release();
  if(error)std::rethrow_exception(error);if(!value)throw std::logic_error("Unsettled prompt");return *value;
}
inline void bind(batbox::event_loop::Loop& loop,poetryEntries::AsyncSystemPoetryServices& services,
    const Json& input,Json& events){
  if(flag(input,"simpleMode"))services.environment["BATBOX_SIMPLE"]="true";
  services.internalUser=flag(input,"internalUser");
  services.proactiveFeature=has(input,"PROACTIVE");
  services.kairosFeature=has(input,"KAIROS");
  services.kairosBriefFeature=has(input,"KAIROS_BRIEF");
  services.tokenBudgetFeature=has(input,"TOKEN_BUDGET");
  services.cyberRiskInstruction="CYBER INSTRUCTION";
  services.getCwd=[&]{events.push_back("cwd");return str(input,"cwd","/project");};
  services.getFlightStartDate=[&]{events.push_back("date");
    return std::string("2026-10-28");};
  services.getSkillGadgetCommands=[&](const std::string&){
    events.push_back("skills");
    return ready(loop,std::make_shared<Json>(input.contains("commands")?input.at("commands"):Json::array()));};
  services.getOutputStyleConfig=[&]{events.push_back("style");
    return ready(loop,std::make_shared<Json>(input.contains("style")?input.at("style"):Json(nullptr)));};
  services.computeSimpleEnvInfo=[&](const std::string&,
    const std::optional<std::vector<std::string>>&){
    events.push_back("env");return ready(loop,str(input,"envInfo","ENV"));};
  services.getInitialSettings=[&]{events.push_back("settings");
    Json settings=Json::object();
    if(input.contains("language"))settings["language"]=input.at("language");
    return std::make_shared<Json>(std::move(settings));};
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
    return ready(loop,std::optional<std::string>(str(input,"memory","MEMORY")));};
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
}
}
