#include "skills/Skill.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

namespace {
using batbox::Json;
using batbox::skills::SkillCommand;
Json project(const SkillCommand& command){
  Json row={{"type","prompt"},{"name",command.name},{"description",command.description},
    {"hasUserSpecifiedDescription",command.hasUserSpecifiedDescription},
    {"allowedTools",command.allowedGadgets},{"disableModelInvocation",command.disableModelInvocation},
    {"userInvocable",command.userInvocable},{"contentLength",command.contentLength},
    {"isHidden",command.isHidden},{"progressMessage",command.progressMessage},
    {"source",command.source},{"loadedFrom",command.loadedFrom},
    {"userFacingName",batbox::skills::userFacingName(command)}};
  if(command.argumentHint)row["argumentHint"]=*command.argumentHint;
  if(!command.argumentNames.empty())row["argNames"]=command.argumentNames;
  if(!command.whenToUse.is_null())row["whenToUse"]=command.whenToUse;
  if(!command.version.is_null())row["version"]=command.version;
  if(command.model)row["model"]=*command.model;
  if(command.executionContext)row["context"]=*command.executionContext;
  if(!command.bat.is_null())row["agent"]=command.bat;
  if(!command.effort.is_null())row["effort"]=command.effort;
  if(!command.paths.empty())row["paths"]=command.paths;
  if(!command.hooks.is_null())row["hooks"]=command.hooks;
  if(command.baseDir)row["skillRoot"]=*command.baseDir;
  return row;
}
Json fields(const batbox::skills::SkillFields& value){
  Json row={{"description",value.description},
    {"hasUserSpecifiedDescription",value.hasUserSpecifiedDescription},
    {"allowedTools",value.allowedGadgets},{"argumentNames",value.argumentNames},
    {"disableModelInvocation",value.disableModelInvocation},
    {"userInvocable",value.userInvocable}};
  if(value.displayName)row["displayName"]=*value.displayName;
  if(value.argumentHint)row["argumentHint"]=*value.argumentHint;
  if(!value.whenToUse.is_null())row["whenToUse"]=value.whenToUse;
  if(!value.version.is_null())row["version"]=value.version;
  if(value.model)row["model"]=*value.model;
  if(!value.hooks.is_null())row["hooks"]=value.hooks;
  if(value.executionContext)row["executionContext"]=*value.executionContext;
  if(!value.bat.is_null())row["agent"]=value.bat;
  if(!value.effort.is_null())row["effort"]=value.effort;
  if(value.shell)row["shell"]=*value.shell;
  return row;
}
}
int main(){
  std::string line;
  while(std::getline(std::cin,line))try{
    const auto input=batbox::value::parse(line);
    const auto op=input.at("op").get<std::string>();
    batbox::skills::SkillServices services;
    services.parseUserSpecifiedModel=[](const std::string& text){return text;};
    services.parseHooks=[](const Json& hooks){return std::optional<Json>(hooks);};
    services.tryParseShellCommand=[](const std::string& args){
      std::vector<std::string> tokens;std::string current;
      for(char c:args){if(c==' '||c=='\t'){
        if(!current.empty()){tokens.push_back(current);current.clear();}
      }else current+=c;}
      if(!current.empty())tokens.push_back(current);
      return std::optional<std::vector<std::string>>(tokens);
    };
    services.executeShellCommandsInPoetry=[](const std::string& content,
      const std::string&,const std::optional<std::string>&,
      const std::vector<std::string>&){return content;};
    services.getFlightId=[](){return "session-1";};
    Json result=nullptr;
    if(op=="fields")result=fields(batbox::skills::parseSkillFrontmatterFields(
      input.at("frontmatter"),input.at("content"),input.at("name"),
      input.value("fallback",std::string("Skill")),services));
    else if(op=="paths"){
      const auto value=batbox::skills::parseSkillPaths(input.at("frontmatter"));
      if(!value.empty())result=value;
    }else if(op=="args")result=batbox::skills::parseArgumentNames(input.at("value"));
    else if(op=="tools")result=batbox::skills::parseSlashCommandGadgetsFromFrontmatter(
      input.at("value"));
    else if(op=="effort")result=batbox::skills::parseEffortValue(input.at("value"));
    else if(op=="substitute"){
      std::optional<std::string> args;
      if(input.contains("args")&&!input.at("args").is_null())args=input.at("args");
      result=batbox::skills::substituteArguments(input.at("content"),args,
        input.value("append",true),input.value("names",std::vector<std::string>{}),services);
    }
    else if(op=="path")result=batbox::skills::getSkillsPath(input.at("source"),
      input.at("dir"),"/managed","/home/test/.batbox");
    else if(op=="identity"){
      const auto id=batbox::skills::getFileIdentity(input.at("filePath"));
      if(id)result=*id;
    }else if(op=="load"){
      result=Json::array();
      for(const auto& entry:batbox::skills::loadSkillsFromSkillsDir(
        input.at("directory"),input.value("source",std::string("projectSettings")),services))
        result.push_back({{"skill",project(entry.skill)},{"filePath",entry.filePath}});
    }else if(op=="command"){
      const auto parsed=batbox::skills::parseSkillFrontmatterFields(
        input.at("frontmatter"),input.at("content"),input.at("name"),"Skill",services);
      std::optional<std::string> baseDir;
      if(input.contains("baseDir")&&!input.at("baseDir").is_null())
        baseDir=input.at("baseDir").get<std::string>();
      auto command=batbox::skills::createSkillCommand(parsed,input.at("name"),
        input.at("content"),"projectSettings",baseDir,
        input.value("loadedFrom",std::string("skills")),
        batbox::skills::parseSkillPaths(input.at("frontmatter")));
      std::optional<std::string> args;
      if(input.contains("args")&&!input.at("args").is_null())args=input.at("args");
      result={{"skill",project(command)},{"prompt",batbox::skills::getPoetryForCommand(
        command,args,services)}};
    }
    std::cout<<batbox::value::stringify({{"result",result}})<<'\n';
  }catch(const std::exception& error){
    std::cout<<batbox::value::stringify({{"error",error.what()}})<<'\n';
  }
}
