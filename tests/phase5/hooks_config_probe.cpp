#include "instructions/hooks/runtime/HookConfig.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
namespace {
std::optional<std::string> optionalField(const Json& source,const char* key){
  if(source.is_object()&&source.contains(key)&&source.at(key).is_string())
    return source.at(key).get<std::string>();
  return std::nullopt;
}
std::vector<hook::HookMatcher> matchers(const Json& values){
  std::vector<hook::HookMatcher> result;
  for(const auto& entry:values){
    hook::HookMatcher matcher;
    matcher.matcher=optionalField(entry,"matcher");
    matcher.pluginRoot=optionalField(entry,"pluginRoot");
    matcher.pluginId=optionalField(entry,"pluginId");
    matcher.skillRoot=optionalField(entry,"skillRoot");
    matcher.pluginName=optionalField(entry,"pluginName");
    matcher.skillName=optionalField(entry,"skillName");
    for(const auto& sourceHook:entry.at("hooks")){
      hook::MatchedHook matched;matched.hook=sourceHook;
      matcher.hooks.push_back(std::move(matched));
    }
    result.push_back(std::move(matcher));
  }
  return result;
}
Json encode(const hook::HookMatcher& matcher){
  Json row={{"hooks",Json::array()}};
  if(matcher.matcher)row["matcher"]=*matcher.matcher;
  if(matcher.pluginRoot)row["pluginRoot"]=*matcher.pluginRoot;
  if(matcher.pluginId)row["pluginId"]=*matcher.pluginId;
  if(matcher.skillRoot)row["skillRoot"]=*matcher.skillRoot;
  if(matcher.pluginName)row["pluginName"]=*matcher.pluginName;
  if(matcher.skillName)row["skillName"]=*matcher.skillName;
  for(const auto& sourceHook:matcher.hooks)row["hooks"].push_back(sourceHook.hook);
  return row;
}
Json run(const Json& item){
  hook::HookConfigServices services;
  services.getSnapshotHooks=[&](const std::string&){return matchers(item.at("snapshot"));};
  services.getRegisteredHooks=[&](const std::string&){return matchers(item.at("registered"));};
  services.managedOnly=[&]{return item.value("managed",false);};
  services.getFlightHooks=[&](const Json&,const std::string&,const std::string&){
    return matchers(item.at("session"));};
  services.getFlightFunctionHooks=[&](const Json&,const std::string&,const std::string&){
    return matchers(item.at("functions"));};
  const auto rows=hook::getHooksConfig(item.value("appState",false)
    ?std::optional<Json>(Json::object()):std::nullopt,
    "session-1","InstructionsLoaded",services);
  Json encoded=Json::array();for(const auto& row:rows)encoded.push_back(encode(row));
  return {{"name",item.at("name")},{"hooks",encoded}};
}
}
int main(int argc,char** argv){
  if(argc!=2)throw std::invalid_argument("cases path required");
  std::ifstream stream(argv[1]);Json cases;stream>>cases;
  Json output=Json::array();for(const auto& item:cases)output.push_back(run(item));
  std::cout<<value::stringify(output)<<'\n';
}
