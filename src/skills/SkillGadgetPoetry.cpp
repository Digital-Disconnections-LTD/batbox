#include "skills/SkillGadgetPoetry.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <regex>
#include <stdexcept>

namespace batbox::skills {
namespace {
constexpr double contextPercent=0.01,charsPerToken=4,defaultBudget=8000;
constexpr double maxListingDescriptionChars=250,minDescriptionLength=20;
std::string jsText(const Json& value){
  if(value.is_string())return value.get<std::string>();
  if(value.is_number())return value::numberToString(value.get<double>());
  if(value.is_boolean())return value.get<bool>()?"true":"false";
  if(value.is_null())return "null";
  if(value.is_array()){
    std::string result;bool first=true;
    for(const auto& item:value){if(!first)result+=',';first=false;
      if(!item.is_null())result+=jsText(item);}
    return result;
  }
  return "[object Object]";
}
double numberFromEnvironment(const std::optional<std::string>& env){
  if(!env)return std::numeric_limits<double>::quiet_NaN();
  const auto trimmed=value::trim(*env);
  if(trimmed.empty())return 0;
  static const std::regex decimal(R"(^[+-]?(?:Infinity|(?:(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?))$)");
  if(std::regex_match(trimmed,decimal)){
    char* end=nullptr;
    return std::strtod(trimmed.c_str(),&end);
  }
  try{
    if(trimmed.size()>2&&trimmed[0]=='0'){
      const int base=trimmed[1]=='x'||trimmed[1]=='X'?16:
        trimmed[1]=='o'||trimmed[1]=='O'?8:
        trimmed[1]=='b'||trimmed[1]=='B'?2:0;
      if(base){std::size_t read=0;const auto value=std::stoull(trimmed.substr(2),&read,base);
        if(read==trimmed.size()-2)return static_cast<double>(value);}
    }
  }catch(const std::exception&){}
  return std::numeric_limits<double>::quiet_NaN();
}
std::string join(const std::vector<std::string>& lines){
  std::string result;bool first=true;
  for(const auto& line:lines){if(!first)result+='\n';first=false;result+=line;}
  return result;
}
}
double getSkillCharBudget(std::optional<double> contextWindowTokens,
  const std::optional<std::string>& environment){
  const auto configured=numberFromEnvironment(environment);
  if(!std::isnan(configured)&&configured!=0)return configured;
  if(contextWindowTokens&&*contextWindowTokens!=0)
    return std::floor(*contextWindowTokens*charsPerToken*contextPercent);
  return defaultBudget;
}
std::string getCommandDescription(const Json& command){
  std::string description=jsText(value::property(command,"description"));
  const auto& when=value::property(command,"whenToUse");
  if(value::truthy(when))description+=" - "+jsText(when);
  return value::stringLength(description)>maxListingDescriptionChars
    ?value::slice(description,0,maxListingDescriptionChars-1)+"…":description;
}
std::string formatCommandDescription(const Json& command){
  return "- "+jsText(value::property(command,"name"))+": "+getCommandDescription(command);
}
std::string formatCommandsWithinBudget(const std::vector<Json>& commands,
  std::optional<double> contextWindowTokens,const SkillPoetryServices& services){
  if(commands.empty())return {};
  if(!services.stringWidth)throw std::invalid_argument("Missing SkillTool binding: stringWidth");
  const auto budget=getSkillCharBudget(contextWindowTokens,
    services.charBudgetEnvironment);
  std::vector<std::string> full;double total=0;
  for(const auto& command:commands){
    full.push_back(formatCommandDescription(command));
    total+=services.stringWidth(full.back());
  }
  total+=static_cast<double>(full.size()-1);
  if(total<=budget)return join(full);
  std::vector<bool> bundled(commands.size(),false);
  std::vector<std::size_t> rest;
  for(std::size_t i=0;i<commands.size();++i){
    bundled[i]=value::property(commands[i],"type")=="prompt"&&
      value::property(commands[i],"source")=="bundled";
    if(!bundled[i])rest.push_back(i);
  }
  double bundledChars=0;
  for(std::size_t i=0;i<commands.size();++i)
    if(bundled[i])bundledChars+=services.stringWidth(full[i])+1;
  if(rest.empty())return join(full);
  double overhead=static_cast<double>(rest.size()-1);
  for(const auto index:rest)
    overhead+=services.stringWidth(jsText(value::property(commands[index],"name")))+4;
  const auto maxDescLength=std::floor((budget-bundledChars-overhead)/rest.size());
  std::vector<std::string> result;
  for(std::size_t i=0;i<commands.size();++i){
    if(bundled[i]){result.push_back(full[i]);continue;}
    const auto name=jsText(value::property(commands[i],"name"));
    if(maxDescLength<minDescriptionLength)result.push_back("- "+name);
    else{
      if(!services.truncate)throw std::invalid_argument("Missing SkillTool binding: truncate");
      result.push_back("- "+name+": "+services.truncate(
        getCommandDescription(commands[i]),maxDescLength));
    }
  }
  return join(result);
}
SkillInfo getSkillGadgetInfo(const std::string& cwd,const SkillPoetryServices& services){
  if(!services.getSkillGadgetCommands)
    throw std::invalid_argument("Missing SkillTool binding: getSkillToolCommands");
  const auto commands=services.getSkillGadgetCommands(cwd);
  return {commands.size(),commands.size()};
}
std::vector<Json> getLimitedSkillGadgetCommands(const std::string& cwd,
  const SkillPoetryServices& services){
  if(!services.getSkillGadgetCommands)
    throw std::invalid_argument("Missing SkillTool binding: getSkillToolCommands");
  return services.getSkillGadgetCommands(cwd);
}
SkillInfo getSkillInfo(const std::string& cwd,const SkillPoetryServices& services){
  try{
    if(!services.getSlashCommandGadgetSkills)
      throw std::invalid_argument("Missing SkillTool binding: getSlashCommandToolSkills");
    const auto skills=services.getSlashCommandGadgetSkills(cwd);
    return {skills.size(),skills.size()};
  }catch(const std::exception& error){
    if(services.logError)services.logError(error.what());
    return {};
  }
}
}
