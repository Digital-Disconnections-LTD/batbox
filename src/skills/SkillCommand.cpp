#include "skills/Skill.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/IndigenousPath.hpp"
#include <cmath>
#include <filesystem>
#include <stdexcept>

namespace batbox::skills {
namespace {
std::string jsText(const Json& value){
  if(value.is_string())return value.get<std::string>();
  if(value.is_boolean())return value.get<bool>()?"true":"false";
  if(value.is_number())return value::numberToString(value.get<double>());
  if(value.is_null())return "null";
  if(value.is_array()){
    std::string result;bool first=true;
    for(const auto& item:value){if(!first)result+=',';first=false;
      if(!item.is_null())result+=jsText(item);}
    return result;
  }
  return "[object Object]";
}
void replaceAll(std::string& text,const std::string& needle,const std::string& value){
  if(needle.empty())return;
  for(std::size_t pos=0;(pos=text.find(needle,pos))!=std::string::npos;pos+=value.size())
    text.replace(pos,needle.size(),value);
}
}
SkillCommand createSkillCommand(const SkillFields& fields,
  const std::string& skillName,const std::string& markdownContent,
  const std::string& source,const std::optional<std::string>& baseDir,
  const std::string& loadedFrom,const std::vector<std::string>& paths){
  SkillCommand result;
  result.name=skillName;
  result.description=fields.description;
  result.markdownContent=markdownContent;
  result.source=source;
  result.loadedFrom=loadedFrom;
  result.displayName=fields.displayName;
  result.hasUserSpecifiedDescription=fields.hasUserSpecifiedDescription;
  result.allowedGadgets=fields.allowedGadgets;
  result.argumentHint=fields.argumentHint;
  result.argumentNames=fields.argumentNames;
  result.whenToUse=fields.whenToUse;
  result.version=fields.version;
  result.model=fields.model;
  result.disableModelInvocation=fields.disableModelInvocation;
  result.userInvocable=fields.userInvocable;
  result.executionContext=fields.executionContext;
  result.bat=fields.bat;
  result.effort=fields.effort;
  result.paths=paths;
  result.contentLength=value::stringLength(markdownContent);
  result.isHidden=!fields.userInvocable;
  result.hooks=fields.hooks;
  result.baseDir=baseDir;
  result.shell=fields.shell;
  return result;
}
std::vector<Json> getPoetryForCommand(const SkillCommand& command,
  const std::optional<std::string>& args,const SkillServices& services){
  std::string finalContent=command.baseDir
    ?"Base directory for this skill: "+*command.baseDir+"\n\n"+command.markdownContent
    :command.markdownContent;
  finalContent=substituteArguments(finalContent,args,true,command.argumentNames,services);
  if(command.baseDir)replaceAll(finalContent,services.skillDirPlaceholder,*command.baseDir);
  if(!services.getFlightId)
    throw std::invalid_argument("Missing skill binding: getSessionId");
  const auto index=finalContent.find(services.flightIdPlaceholder);
  if(index!=std::string::npos)finalContent.replace(index,
    services.flightIdPlaceholder.size(),services.getFlightId());
  else (void)services.getFlightId();
  if(command.loadedFrom!="mcp"){
    if(!services.executeShellCommandsInPoetry)
      throw std::invalid_argument("Missing skill binding: executeShellCommandsInPrompt");
    finalContent=services.executeShellCommandsInPoetry(finalContent,
      "/"+command.name,command.shell,command.allowedGadgets);
  }
  return {{{"type","text"},{"text",finalContent}}};
}
std::string getSkillsPath(const std::string& source,const std::string& dir,
  const std::string& managedPath,const std::string& configHomeDir){
  if(source=="policySettings")
    return runtime::pathToUtf8(runtime::pathFromUtf8(managedPath)/
      ".batbox"/runtime::pathFromUtf8(dir));
  if(source=="userSettings")return runtime::pathToUtf8(
    runtime::pathFromUtf8(configHomeDir)/runtime::pathFromUtf8(dir));
  if(source=="projectSettings")return ".batbox/"+dir;
  if(source=="plugin")return "plugin";
  return {};
}
std::optional<std::string> getFileIdentity(const std::string& filePath){
  try{return runtime::pathToUtf8(std::filesystem::canonical(
    runtime::pathFromUtf8(filePath)));}
  catch(const std::exception&){return std::nullopt;}
}
double estimateSkillFrontmatterTokens(const SkillCommand& command){
  std::string combined;
  for(const std::string* item:{&command.name,&command.description})
    if(!item->empty()){if(!combined.empty())combined+=' ';combined+=*item;}
  if(value::truthy(command.whenToUse)){
    const auto value=jsText(command.whenToUse);
    if(!value.empty()){if(!combined.empty())combined+=' ';combined+=value;}
  }
  return value::round(static_cast<double>(value::stringLength(combined))/4);
}
std::string userFacingName(const SkillCommand& command){
  return command.displayName&&!command.displayName->empty()
    ?*command.displayName:command.name;
}
}
