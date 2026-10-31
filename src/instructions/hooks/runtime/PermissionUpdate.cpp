#include "instructions/hooks/runtime/PermissionUpdate.hpp"

namespace batbox::instructions::hooks::runtime {
namespace {
bool oneOf(const Json& input,std::initializer_list<const char*> values){
  if(!input.is_string())return false;
  for(const auto* value:values)if(input==value)return true;
  return false;
}
std::optional<Json> rules(const Json& input){
  if(!input.is_array())return std::nullopt;
  Json result=Json::array();
  for(const auto& item:input){
    if(!item.is_object()||!item.contains("toolName")||
      !item.at("toolName").is_string())return std::nullopt;
    Json rule={{"toolName",item.at("toolName")}};
    if(item.contains("ruleContent")){
      if(!item.at("ruleContent").is_string())return std::nullopt;
      rule["ruleContent"]=item.at("ruleContent");
    }
    result.push_back(std::move(rule));
  }
  return result;
}
bool destination(const Json& input){
  return oneOf(input,{"userSettings","projectSettings","localSettings",
    "session","cliArg"});
}
}
std::optional<Json> parseHookPermissionUpdate(const Json& input){
  if(!input.is_object()||!input.contains("type")||
    !input.at("type").is_string()||!input.contains("destination")||
    !destination(input.at("destination")))return std::nullopt;
  const auto type=input.at("type").get<std::string>();
  Json output={{"type",type}};
  if(type=="addRules"||type=="replaceRules"||type=="removeRules"){
    if(!input.contains("rules")||!input.contains("behavior")||
      !oneOf(input.at("behavior"),{"allow","deny","ask"}))
      return std::nullopt;
    const auto parsed=rules(input.at("rules"));
    if(!parsed)return std::nullopt;
    output["rules"]=*parsed;
    output["behavior"]=input.at("behavior");
  }else if(type=="setMode"){
    if(!input.contains("mode")||
      !oneOf(input.at("mode"),{"acceptEdits","bypassPermissions",
        "default","dontAsk","plan"}))return std::nullopt;
    output["mode"]=input.at("mode");
  }else if(type=="addDirectories"||type=="removeDirectories"){
    if(!input.contains("directories")||
      !input.at("directories").is_array())return std::nullopt;
    for(const auto& path:input.at("directories"))
      if(!path.is_string())return std::nullopt;
    output["directories"]=input.at("directories");
  }else return std::nullopt;
  output["destination"]=input.at("destination");
  return output;
}
} // namespace batbox::instructions::hooks::runtime
