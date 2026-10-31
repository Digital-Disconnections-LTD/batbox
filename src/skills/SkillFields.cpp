#include "skills/Skill.hpp"
#include "instructions/Frontmatter.hpp"
#include <algorithm>
#include <cmath>
#include <regex>
#include <stdexcept>

namespace batbox::skills {
namespace {
std::string jsText(const Json& value){
  if(value.is_string())return value.get<std::string>();
  if(value.is_boolean())return value.get<bool>()?"true":"false";
  if(value.is_number())return value::numberToString(value.get<double>());
  if(value.is_null())return "null";
  if(value.is_array()){
    std::string output;bool first=true;
    for(const auto& item:value){if(!first)output+=',';first=false;
      if(!item.is_null())output+=jsText(item);}
    return output;
  }
  return "[object Object]";
}
bool numericName(const std::string& value){
  return !value.empty()&&std::all_of(value.begin(),value.end(),
    [](unsigned char c){return c>='0'&&c<='9';});
}
void addSplitGadgets(const std::string& text,std::vector<std::string>& result){
  std::string current;bool inParens=false;
  for(char c:text){
    if(c=='('){inParens=true;current+=c;}
    else if(c==')'){inParens=false;current+=c;}
    else if(c==','&&!inParens){
      const auto trimmed=value::trim(current);
      if(!trimmed.empty())result.push_back(trimmed);
      current.clear();
    }else if(c==' '&&!inParens){
      const auto trimmed=value::trim(current);
      if(!trimmed.empty()){result.push_back(trimmed);current.clear();}
    }else current+=c;
  }
  const auto trimmed=value::trim(current);
  if(!trimmed.empty())result.push_back(trimmed);
}
}
std::vector<std::string> parseSkillPaths(const Json& frontmatter){
  const auto& raw=value::property(frontmatter,"paths");
  if(!value::truthy(raw))return {};
  auto patterns=instructions::splitPathInFrontmatter(raw);
  std::vector<std::string> result;
  for(auto& pattern:patterns){
    if(pattern.ends_with("/**"))pattern.resize(pattern.size()-3);
    if(!pattern.empty())result.push_back(pattern);
  }
  if(result.empty()||std::all_of(result.begin(),result.end(),
      [](const auto& item){return item=="**";}))return {};
  return result;
}
std::vector<std::string> parseArgumentNames(const Json& names){
  std::vector<std::string> result;
  if(names.is_array()){
    for(const auto& item:names)if(item.is_string()){
      auto text=item.get<std::string>();
      if(!value::trim(text).empty()&&!numericName(text))result.push_back(std::move(text));
    }
  }else if(names.is_string()){
    static const std::regex whitespace(R"(\s+)");
    const auto raw=names.get<std::string>();
    for(std::sregex_token_iterator it(raw.begin(),raw.end(),whitespace,-1),end;
        it!=end;++it){
      const auto text=it->str();
      if(!value::trim(text).empty()&&!numericName(text))result.push_back(text);
    }
  }
  return result;
}
std::vector<std::string> parseSlashCommandGadgetsFromFrontmatter(const Json& gadgets){
  std::vector<std::string> result;
  if(!value::truthy(gadgets))return result;
  if(gadgets.is_string())addSplitGadgets(gadgets.get<std::string>(),result);
  else if(gadgets.is_array())for(const auto& item:gadgets)
    if(item.is_string())addSplitGadgets(item.get<std::string>(),result);
  if(std::find(result.begin(),result.end(),"*")!=result.end())return {"*"};
  return result;
}
Json parseEffortValue(const Json& value){
  if(value.is_null()||value=="")return nullptr;
  if(value.is_number()){
    const double number=value.get<double>();
    if(std::isfinite(number)&&std::floor(number)==number)return value;
  }
  auto lower=jsText(value);
  std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){
    return static_cast<char>(std::tolower(c));});
  if(lower=="low"||lower=="medium"||lower=="high"||lower=="max")return lower;
  const double number=value::parseInt10(lower);
  if(std::isfinite(number)&&std::floor(number)==number)return number;
  return nullptr;
}
SkillFields parseSkillFrontmatterFields(const Json& frontmatter,
  const std::string& markdownContent,const std::string& resolvedName,
  const std::string& descriptionFallbackLabel,const SkillServices& services){
  SkillFields result;
  const auto& rawDescription=value::property(frontmatter,"description");
  const auto validated=instructions::coerceDescriptionToString(rawDescription);
  result.description=validated.value_or(instructions::extractDescriptionFromMarkdown(
    markdownContent,descriptionFallbackLabel));
  result.hasUserSpecifiedDescription=validated.has_value();
  const auto& userInvocable=value::property(frontmatter,"user-invocable");
  result.userInvocable=frontmatter.is_object()&&frontmatter.contains("user-invocable")
    ?instructions::parseBooleanFrontmatter(userInvocable):true;
  const auto& model=value::property(frontmatter,"model");
  if(model!="inherit"&&value::truthy(model)){
    if(!services.parseUserSpecifiedModel)
      throw std::invalid_argument("Missing skill binding: parseUserSpecifiedModel");
    result.model=services.parseUserSpecifiedModel(jsText(model));
  }
  const auto& effort=value::property(frontmatter,"effort");
  if(frontmatter.is_object()&&frontmatter.contains("effort")){
    result.effort=parseEffortValue(effort);
    if(result.effort.is_null()&&services.logDebug)services.logDebug(
      "Skill "+resolvedName+" has invalid effort '"+jsText(effort)+
      "'. Valid options: low, medium, high, max or an integer");
  }
  const auto& name=value::property(frontmatter,"name");
  if(!name.is_null())result.displayName=jsText(name);
  result.allowedGadgets=parseSlashCommandGadgetsFromFrontmatter(
    value::property(frontmatter,"allowed-tools"));
  const auto& hint=value::property(frontmatter,"argument-hint");
  if(!hint.is_null())result.argumentHint=jsText(hint);
  result.argumentNames=parseArgumentNames(value::property(frontmatter,"arguments"));
  result.whenToUse=value::property(frontmatter,"when_to_use");
  result.version=value::property(frontmatter,"version");
  result.disableModelInvocation=instructions::parseBooleanFrontmatter(
    value::property(frontmatter,"disable-model-invocation"));
  const auto& hooks=value::property(frontmatter,"hooks");
  if(value::truthy(hooks)){
    if(!services.parseHooks)throw std::invalid_argument("Missing skill binding: parseHooks");
    const auto parsed=services.parseHooks(hooks);
    if(parsed)result.hooks=*parsed;
  }
  if(value::property(frontmatter,"context")=="fork")result.executionContext="fork";
  result.bat=value::property(frontmatter,"agent");
  result.shell=instructions::parseShellFrontmatter(
    value::property(frontmatter,"shell"),resolvedName,services.logDebug);
  return result;
}
}
