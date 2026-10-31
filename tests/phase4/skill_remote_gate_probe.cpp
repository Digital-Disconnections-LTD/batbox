#include "skills/remote/RemoteSkill.hpp"
#include "skills/SkillGadgetValidation.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using batbox::Json;
namespace remote=batbox::skills::remote;
namespace skills=batbox::skills;
namespace value=batbox::value;
namespace {
std::string str(const Json& object,const char* key){
  const auto& raw=value::property(object,key);
  return raw.is_string()?raw.get<std::string>():std::string{};
}
std::vector<Json> commands(const Json& input){
  std::vector<Json> result;
  const auto& raw=value::property(input,"commands");
  if(raw.is_array())for(const auto& item:raw)result.push_back(item);
  return result;
}
skills::RuleEntries rules(const Json& input,const char* key){
  skills::RuleEntries result;
  const auto& raw=value::property(input,key);
  if(raw.is_array())for(const auto& item:raw)
    result.emplace_back(str(item,"content"),value::property(item,"rule"));
  return result;
}
Json validation(const batbox::gadgets::ValidationResult& value){
  Json out={{"result",value.result}};
  if(value.message)out["message"]=*value.message;
  if(value.errorCode)out["errorCode"]=*value.errorCode;
  return out;
}
Json run(const Json& input){
  remote::Services service;
  service.enabled=[]{return true;};
  service.stripCanonicalPrefix=[](const std::string& name)->std::optional<std::string>{
    const std::string prefix="_canonical_";
    if(name.starts_with(prefix))return name.substr(prefix.size());
    return std::nullopt;
  };
  service.getDiscoveredRemoteSkill=[&](const std::string& slug)
      ->std::optional<remote::DiscoveredSkill>{
    const auto& meta=value::property(value::property(input,"discovered"),slug);
    return meta.is_null()?std::nullopt:
      std::optional<remote::DiscoveredSkill>(remote::DiscoveredSkill{str(meta,"url")});
  };
  skills::SkillGadgetLookup lookup;
  lookup.findCommand=[](const std::string& name,const std::vector<Json>& all){
    for(const auto& item:all)if(value::property(item,"name")==name)return item;
    return Json(nullptr);
  };
  const auto all=commands(input);
  try{
    Json result;
    if(value::property(input,"op")=="validate"){
      const auto skill=str(input,"skill");
      const auto trimmed=value::trim(skill);
      if(trimmed.empty())result=validation(skills::validateSkillInput(skill,all,lookup));
      else{
        const auto name=trimmed.starts_with('/')?value::substring(trimmed,1):trimmed;
        const auto remoteResult=remote::validateRemoteSkill(name,service);
        result=validation(remoteResult?*remoteResult:
          skills::validateSkillInput(skill,all,lookup));
      }
    }else{
      const auto isRemote=[&](const std::string& name){
        return remote::remoteSlugForCommand(name,service).has_value();
      };
      result=skills::checkSkillPermissions(value::property(input,"input"),all,
        rules(input,"deny"),rules(input,"allow"),lookup,"Skill",isRemote);
    }
    return {{"result",result}};
  }catch(const std::exception& error){return {{"error",error.what()}};}
}
}
int main(){
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty())
    std::cout<<value::stringify(run(value::parse(line)))<<'\n';
}
