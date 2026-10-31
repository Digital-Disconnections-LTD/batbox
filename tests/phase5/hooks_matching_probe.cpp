#include "instructions/hooks/runtime/HookMatching.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
namespace {
std::optional<std::string> optionalField(const Json& value,const char* key){
  if(value.is_object()&&value.contains(key)&&value.at(key).is_string())
    return value.at(key).get<std::string>();
  return std::nullopt;
}
hook::HookKind kind(const std::string& type){
  if(type=="callback")return hook::HookKind::Callback;
  if(type=="prompt")return hook::HookKind::Poetry;
  if(type=="agent")return hook::HookKind::Bat;
  if(type=="function")return hook::HookKind::Function;
  if(type=="http")return hook::HookKind::Http;
  return hook::HookKind::Command;
}
Json run(const Json& item){
  event_loop::Loop loop;Json logs=Json::array();
  hook::HookMatchServices services;
  services.getHooksConfig=[&](const std::optional<Json>&,const std::string&,
    const std::string&){
    if(item.value("configThrows",false))throw std::runtime_error("config");
    std::vector<hook::HookMatcher> matchers;
    for(const auto& entry:item.at("matchers")){
      hook::HookMatcher row;
      row.matcher=optionalField(entry,"matcher");
      row.pluginRoot=optionalField(entry,"pluginRoot");
      row.pluginId=optionalField(entry,"pluginId");
      row.skillRoot=optionalField(entry,"skillRoot");
      row.pluginName=optionalField(entry,"pluginName");
      row.skillName=optionalField(entry,"skillName");
      for(const auto& sourceHook:entry.at("hooks")){
        hook::MatchedHook configured;
        configured.kind=kind(sourceHook.at("type").get<std::string>());
        configured.hook=sourceHook;row.hooks.push_back(std::move(configured));
      }
      matchers.push_back(std::move(row));
    }
    return matchers;
  };
  services.normalizeLegacyGadgetName=[](const std::string& name){
    return name=="Task"?std::string("Agent"):name;};
  services.getLegacyGadgetNames=[](const std::string& name){
    return name=="Agent"?std::vector<std::string>{"Task"}:
      std::vector<std::string>{};};
  services.logDebug=[&](const std::string& message,const std::string& level){
    logs.push_back(level+":"+message);};
  services.prepareIfConditionMatcher=[&](const Json& input){
    event_loop::Deferred<std::optional<hook::IfConditionMatcher>> result(loop);
    const auto event=input.at("hook_event_name").get<std::string>();
    if(event=="PreToolUse"||event=="PostToolUse"||
      event=="PostToolUseFailure"||event=="PermissionRequest"){
      result.resolve(hook::IfConditionMatcher([match=item.value("ifMatches",false)](
        const std::string&){return match;}));
    }else result.resolve(std::nullopt);
    return result;
  };
  Json rows=Json::array();
  hook::getMatchingHooks(loop,std::nullopt,"session-1",
    item.at("event").get<std::string>(),item.at("input"),services).then(
      [&](const std::vector<hook::MatchedHook>* result,
        const std::exception_ptr& error){
        if(error)std::rethrow_exception(error);
        for(const auto& matched:*result){
          Json row={{"hook",matched.hook},{"hookSource",matched.hookSource}};
          if(matched.pluginRoot)row["pluginRoot"]=*matched.pluginRoot;
          if(matched.pluginId)row["pluginId"]=*matched.pluginId;
          if(matched.skillRoot)row["skillRoot"]=*matched.skillRoot;
          rows.push_back(std::move(row));
        }
      });
  loop.run();
  return {{"name",item.at("name")},{"hooks",rows},{"logs",logs}};
}
}
int main(int argc,char** argv){
  if(argc!=2)throw std::invalid_argument("cases path required");
  std::ifstream stream(argv[1]);Json cases;stream>>cases;
  Json rows=Json::array();for(const auto& item:cases)rows.push_back(run(item));
  std::cout<<value::stringify(rows)<<'\n';
}
