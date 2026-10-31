#include "skills/SkillRoster.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

namespace {
using batbox::Json;
Json project(const batbox::skills::SkillCommand& skill){
  Json row={{"type","prompt"},{"name",skill.name},{"description",skill.description},
    {"hasUserSpecifiedDescription",skill.hasUserSpecifiedDescription},
    {"allowedTools",skill.allowedGadgets},{"disableModelInvocation",skill.disableModelInvocation},
    {"userInvocable",skill.userInvocable},{"contentLength",skill.contentLength},
    {"isHidden",skill.isHidden},{"progressMessage",skill.progressMessage},
    {"source",skill.source},{"loadedFrom",skill.loadedFrom},
    {"userFacingName",batbox::skills::userFacingName(skill)}};
  if(skill.argumentHint)row["argumentHint"]=*skill.argumentHint;
  if(!skill.argumentNames.empty())row["argNames"]=skill.argumentNames;
  if(!skill.whenToUse.is_null())row["whenToUse"]=skill.whenToUse;
  if(!skill.version.is_null())row["version"]=skill.version;
  if(skill.model)row["model"]=*skill.model;
  if(skill.executionContext)row["context"]=*skill.executionContext;
  if(!skill.bat.is_null())row["agent"]=skill.bat;
  if(!skill.effort.is_null())row["effort"]=skill.effort;
  if(!skill.paths.empty())row["paths"]=skill.paths;
  if(!skill.hooks.is_null())row["hooks"]=skill.hooks;
  if(skill.baseDir)row["skillRoot"]=*skill.baseDir;
  return row;
}
Json projectAll(const std::vector<batbox::skills::SkillCommand>& skills){
  Json result=Json::array();for(const auto& skill:skills)result.push_back(project(skill));
  return result;
}
}
int main(){
  batbox::skills::SkillRoster roster;
  std::string line;
  while(std::getline(std::cin,line))try{
    const auto input=batbox::value::parse(line);
    const auto config=input.value("config",Json::object());
    batbox::skills::SkillRosterServices services;
    services.managedPath=config.value("managedPath",std::string());
    services.configHomeDir=config.value("configHomeDir",std::string());
    if(const auto pos=services.configHomeDir.find(".claude");pos!=std::string::npos)
      services.configHomeDir.replace(pos,7,".batbox");
    services.homeDir=config.value("homeDir",std::string());
    services.gitRoot=config.value("gitRoot",std::string());
    services.additionalDirs=config.value("additionalDirs",std::vector<std::string>{});
    services.disablePolicySkills=config.value("disablePolicy",false);
    services.userSettingsEnabled=config.value("userEnabled",true);
    services.projectSettingsEnabled=config.value("projectEnabled",true);
    services.skillsLocked=config.value("skillsLocked",false);
    services.bareMode=config.value("bareMode",false);
    services.loadLegacyCommands=[](const std::string&){
      return std::vector<batbox::skills::SkillWithPath>{};};
    services.isPathGitignored=[](const std::string&,const std::string&){return false;};
    services.ignorePatternsMatch=[](const std::vector<std::string>& patterns,
      const std::string& value){
      for(const auto& pattern:patterns)if(value==pattern||
        value.starts_with(pattern+"/"))return true;
      return false;
    };
    const auto op=input.at("op").get<std::string>();
    Json result;
    if(op=="get")result=projectAll(batbox::skills::getSkillDirCommands(
      input.at("cwd"),roster,services));
    else if(op=="projectDirs")result=batbox::skills::getProjectDirsUpToHome(
      "skills",input.at("cwd"),services);
    else if(op=="clearCache"){
      batbox::skills::clearSkillStashes(roster,services);result=true;
    }else if(op=="discover")result=batbox::skills::discoverSkillDirsForPaths(
      input.at("files").get<std::vector<std::string>>(),input.at("cwd"),
      roster,services);
    else if(op=="add"){
      batbox::skills::addSkillDirectories(input.at("dirs").get<std::vector<std::string>>(),
        roster,services);result=true;
    }else if(op=="dynamic")result=projectAll(batbox::skills::getDynamicSkills(roster));
    else if(op=="activate")result=batbox::skills::activateConditionalSkillsForPaths(
      input.at("files").get<std::vector<std::string>>(),input.at("cwd"),
      roster,services);
    else if(op=="count")result=batbox::skills::getConditionalSkillCount(roster);
    else if(op=="clearDynamic"){
      batbox::skills::clearDynamicSkills(roster);result=true;
    }
    std::cout<<batbox::value::stringify({{"result",result}})<<'\n';
  }catch(const std::exception& error){
    std::cout<<batbox::value::stringify({{"error",error.what()}})<<'\n';
  }
}
