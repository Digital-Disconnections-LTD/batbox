#include "settings/PermissionRules.hpp"

namespace batbox::settings {
namespace {
std::string replaceAll(std::string text,const std::string& from,
  const std::string& to){
  std::size_t pos=0;
  while((pos=text.find(from,pos))!=std::string::npos){
    text.replace(pos,from.size(),to);pos+=to.size();
  }
  return text;
}
bool escaped(const std::u16string& text,std::size_t index){
  std::size_t count=0;
  while(index>0&&text[index-1]==u'\\'){--index;++count;}
  return count%2!=0;
}
std::optional<std::size_t> first(const std::u16string& text,char16_t ch){
  for(std::size_t i=0;i<text.size();++i)
    if(text[i]==ch&&!escaped(text,i))return i;
  return std::nullopt;
}
std::optional<std::size_t> last(const std::u16string& text,char16_t ch){
  for(std::size_t i=text.size();i>0;--i)
    if(text[i-1]==ch&&!escaped(text,i-1))return i-1;
  return std::nullopt;
}
}
std::string normalizeLegacyGadgetName(const std::string& name,
  const PermissionRuleServices& services){
  const auto found=services.legacyGadgetNames.find(name);
  return found==services.legacyGadgetNames.end()?name:found->second;
}
std::vector<std::string> getLegacyGadgetNames(const std::string& canonicalName,
  const PermissionRuleServices& services){
  std::vector<std::string> result;
  for(const auto& [legacy,canonical]:services.legacyGadgetNames)
    if(canonical==canonicalName)result.push_back(legacy);
  return result;
}
std::string escapeRuleContent(const std::string& content){
  return replaceAll(replaceAll(replaceAll(content,"\\","\\\\"),
    "(","\\("),")","\\)");
}
std::string unescapeRuleContent(const std::string& content){
  return replaceAll(replaceAll(replaceAll(content,"\\(","("),
    "\\)",")"),"\\\\","\\");
}
PermissionRuleValue permissionRuleValueFromString(const std::string& rule,
  const PermissionRuleServices& services){
  const auto units=value::utf16CodeUnits(rule);
  const auto open=first(units,u'(');
  if(!open)return {normalizeLegacyGadgetName(rule,services),std::nullopt};
  const auto close=last(units,u')');
  if(!close||*close<=*open||*close!=units.size()-1)
    return {normalizeLegacyGadgetName(rule,services),std::nullopt};
  const auto gadget=value::fromUtf16(units.substr(0,*open));
  const auto content=value::fromUtf16(units.substr(*open+1,*close-*open-1));
  if(gadget.empty())return {normalizeLegacyGadgetName(rule,services),std::nullopt};
  if(content.empty()||content=="*")
    return {normalizeLegacyGadgetName(gadget,services),std::nullopt};
  return {normalizeLegacyGadgetName(gadget,services),
    unescapeRuleContent(content)};
}
std::string permissionRuleValueToString(const PermissionRuleValue& value){
  if(!value.ruleContent||value.ruleContent->empty())return value.gadgetName;
  return value.gadgetName+"("+escapeRuleContent(*value.ruleContent)+")";
}
std::optional<std::pair<std::string,std::optional<std::string>>>
  mcpInfoFromString(const std::string& gadgetString){
  const auto first=gadgetString.find("__");
  if(first==std::string::npos||gadgetString.substr(0,first)!="mcp")
    return std::nullopt;
  const auto second=gadgetString.find("__",first+2);
  const auto server=gadgetString.substr(first+2,second==std::string::npos
    ?std::string::npos:second-first-2);
  if(server.empty())return std::nullopt;
  return std::pair{server,second==std::string::npos
    ?std::optional<std::string>{}:std::optional<std::string>(
      gadgetString.substr(second+2))};
}
}
