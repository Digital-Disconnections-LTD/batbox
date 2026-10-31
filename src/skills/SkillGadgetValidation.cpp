#include "skills/SkillGadgetValidation.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace batbox::skills {
namespace {
Json find(const std::string& name,const std::vector<Json>& commands,
  const SkillGadgetLookup& lookup){
  if(!lookup.findCommand)throw std::invalid_argument("Missing SkillTool binding: findCommand");
  return lookup.findCommand(name,commands);
}
std::string commandName(const std::string& skill){
  const auto trimmed=value::trim(skill);
  return trimmed.starts_with('/')?value::substring(trimmed,1):trimmed;
}
Json updatedInput(const Json& input){
  Json result=Json::object();
  result["skill"]=value::property(input,"skill");
  if(input.is_object()&&input.contains("args"))result["args"]=input.at("args");
  return result;
}
}
std::vector<Json> getAllSkillCommands(const std::vector<Json>& localCommands,
  const std::vector<Json>& mcpCommands){
  std::vector<Json> mcpSkills;
  for(const auto& command:mcpCommands)
    if(value::property(command,"type")=="prompt"&&
       value::property(command,"loadedFrom")=="mcp")mcpSkills.push_back(command);
  if(mcpSkills.empty())return localCommands;
  std::set<std::string> seen;
  std::vector<Json> result;
  for(const auto& command:localCommands){
    const auto& name=value::property(command,"name");
    if(!name.is_string()||seen.insert(name.get<std::string>()).second)
      result.push_back(command);
  }
  for(const auto& command:mcpSkills){
    const auto& name=value::property(command,"name");
    if(!name.is_string()||seen.insert(name.get<std::string>()).second)
      result.push_back(command);
  }
  return result;
}
gadgets::ValidationResult validateSkillInput(const std::string& skill,
  const std::vector<Json>& commands,const SkillGadgetLookup& lookup){
  const auto invalid=[](const std::string& message,int code){
    gadgets::ValidationResult result;result.result=false;
    result.message=message;result.errorCode=code;return result;
  };
  const auto trimmed=value::trim(skill);
  if(trimmed.empty())return invalid("Invalid skill format: "+skill,1);
  const auto name=commandName(skill);
  const auto command=find(name,commands,lookup);
  if(command.is_null())return invalid("Unknown skill: "+name,2);
  if(value::truthy(value::property(command,"disableModelInvocation")))
    return invalid("Skill "+name+
      " cannot be used with Skill tool due to disable-model-invocation",4);
  if(value::property(command,"type")!="prompt")
    return invalid("Skill "+name+" is not a prompt-based skill",5);
  return {};
}
bool skillRuleMatches(const std::string& ruleContent,const std::string& commandName){
  const auto normalized=ruleContent.starts_with('/')?ruleContent.substr(1):ruleContent;
  if(normalized==commandName)return true;
  if(normalized.ends_with(":*"))
    return commandName.starts_with(normalized.substr(0,normalized.size()-2));
  return false;
}
bool skillHasOnlySafeProperties(const Json& command){
  static const std::set<std::string> safe={
    "type","progressMessage","contentLength","argNames","model","effort",
    "source","pluginInfo","disableNonInteractive","skillRoot","context",
    "agent","getPromptForCommand","frontmatterKeys","name","description",
    "hasUserSpecifiedDescription","isEnabled","isHidden","aliases","isMcp",
    "argumentHint","whenToUse","paths","version","disableModelInvocation",
    "userInvocable","loadedFrom","immediate","userFacingName"};
  if(!command.is_object())return true;
  for(auto it=command.begin();it!=command.end();++it){
    if(safe.contains(it.key()))continue;
    const auto& value=it.value();
    if(value.is_null()||(value.is_array()&&value.empty())||
       (value.is_object()&&value.empty()))continue;
    return false;
  }
  return true;
}
Json checkSkillPermissions(const Json& input,const std::vector<Json>& commands,
  const RuleEntries& denyRules,const std::function<RuleEntries()>& loadAllowRules,
  const SkillGadgetLookup& lookup,const std::string& gadgetName,
  const std::function<bool(const std::string&)>& isRemoteSkill){
  const auto name=commandName(value::property(input,"skill").get<std::string>());
  const auto command=find(name,commands,lookup);
  for(const auto& [content,rule]:denyRules)if(skillRuleMatches(content,name))
    return {{"behavior","deny"},
      {"message","Skill execution blocked by permission rules"},
      {"decisionReason",{{"type","rule"},{"rule",rule}}}};
  if(isRemoteSkill&&isRemoteSkill(name))
    return {{"behavior","allow"},{"updatedInput",updatedInput(input)}};
  if(!loadAllowRules)throw std::invalid_argument("Missing SkillTool binding: allowRules");
  const auto allowRules=loadAllowRules();
  for(const auto& [content,rule]:allowRules)if(skillRuleMatches(content,name))
    return {{"behavior","allow"},{"updatedInput",updatedInput(input)},
      {"decisionReason",{{"type","rule"},{"rule",rule}}}};
  if(!command.is_null()&&value::property(command,"type")=="prompt"&&
     skillHasOnlySafeProperties(command))
    return {{"behavior","allow"},{"updatedInput",updatedInput(input)}};
  Json suggestions=Json::array();
  for(const auto& ruleContent:{name,name+":*"})
    suggestions.push_back({{"type","addRules"},
      {"rules",Json::array({{{"toolName",gadgetName},
        {"ruleContent",ruleContent}}})},
      {"behavior","allow"},{"destination","localSettings"}});
  Json decision={{"behavior","ask"},{"message","Execute skill: "+name},
    {"suggestions",suggestions},{"updatedInput",updatedInput(input)}};
  if(!command.is_null())decision["metadata"]={{"command",command}};
  return decision;
}
Json checkSkillPermissions(const Json& input,const std::vector<Json>& commands,
  const RuleEntries& denyRules,const RuleEntries& allowRules,
  const SkillGadgetLookup& lookup,const std::string& gadgetName,
  const std::function<bool(const std::string&)>& isRemoteSkill){
  return checkSkillPermissions(input,commands,denyRules,
    [&allowRules]{return allowRules;},lookup,gadgetName,isRemoteSkill);
}
}
