#include "settings/PermissionRules.hpp"
#include <regex>
#include <unicode/ustring.h>

namespace batbox::settings {
namespace {
bool escaped(const std::u16string& text,std::size_t index){
  std::size_t count=0;
  while(index>0&&text[index-1]==u'\\'){--index;++count;}
  return count%2!=0;
}
std::size_t countUnescaped(const std::u16string& text,char16_t ch){
  std::size_t count=0;
  for(std::size_t i=0;i<text.size();++i)
    if(text[i]==ch&&!escaped(text,i))++count;
  return count;
}
bool emptyParens(const std::u16string& text){
  for(std::size_t i=0;i+1<text.size();++i)
    if(text[i]==u'('&&text[i+1]==u')'&&!escaped(text,i))return true;
  return false;
}
std::string upperFirst(const std::string& text){
  auto units=value::utf16CodeUnits(text);
  if(units.empty())return {};
  UErrorCode status=U_ZERO_ERROR;
  UChar result[8]{};
  const auto length=u_strToUpper(result,8,reinterpret_cast<const UChar*>(
    units.data()),1,"",&status);
  if(U_FAILURE(status))return value::fromUtf16(units.substr(0,1));
  return value::fromUtf16(std::u16string(
    reinterpret_cast<const char16_t*>(result),length));
}
std::string capitalize(const std::string& text){
  if(text.empty())return text;
  return upperFirst(text)+value::slice(text,1);
}
bool has(const std::string& text,const std::string& part){
  return text.find(part)!=std::string::npos;
}
}
RuleValidationResult validatePermissionRule(const std::string& rule,
  const PermissionRuleServices& services){
  if(rule.empty()||value::trim(rule).empty())
    return {false,"Permission rule cannot be empty"};
  const auto units=value::utf16CodeUnits(rule);
  if(countUnescaped(units,u'(')!=countUnescaped(units,u')'))
    return {false,"Mismatched parentheses",
      "Ensure all opening parentheses have matching closing parentheses"};
  if(emptyParens(units)){
    const auto first=units.find(u'(');
    const auto gadget=value::fromUtf16(units.substr(0,first));
    if(gadget.empty())return {false,"Empty parentheses with no tool name",
      "Specify a tool name before the parentheses"};
    return {false,"Empty parentheses",
      "Either specify a pattern or use just \""+gadget+"\" without parentheses",
      std::vector<std::string>{gadget,gadget+"(some-pattern)"}};
  }
  const auto parsed=permissionRuleValueFromString(rule,services);
  const auto mcp=mcpInfoFromString(parsed.gadgetName);
  if(mcp){
    if(parsed.ruleContent||countUnescaped(units,u'(')>0){
      std::vector<std::string> examples={"mcp__"+mcp->first,
        "mcp__"+mcp->first+"__*"};
      if(mcp->second&&*mcp->second!="*")examples.push_back(
        "mcp__"+mcp->first+"__"+*mcp->second);
      return {false,"MCP rules do not support patterns in parentheses",
        "Use \""+parsed.gadgetName+"\" without parentheses, or use \"mcp__"+
          mcp->first+"__*\" for all tools",examples};
    }
    return {true};
  }
  if(parsed.gadgetName.empty())return {false,"Tool name cannot be empty"};
  const auto first=value::substring(parsed.gadgetName,0,1);
  if(first!=upperFirst(parsed.gadgetName))
    return {false,"Tool names must start with uppercase",
      "Use \""+capitalize(parsed.gadgetName)+"\""};
  if(parsed.ruleContent){
    if(parsed.gadgetName=="WebSearch"||parsed.gadgetName=="WebFetch"){
      const auto custom=getCustomValidation(parsed.gadgetName,*parsed.ruleContent);
      if(!custom.valid)return custom;
    }
    if(services.customValidation){
      const auto custom=services.customValidation(parsed.gadgetName,
        *parsed.ruleContent);
      if(!custom.valid)return custom;
    }
  }
  if(isBashPrefixGadget(parsed.gadgetName,services)&&parsed.ruleContent){
    const auto& content=*parsed.ruleContent;
    if(has(content,":*")&&!content.ends_with(":*"))
      return {false,"The :* pattern must be at the end",
        "Move :* to the end for prefix matching, or use * for wildcard matching",
        std::vector<std::string>{"Bash(npm run:*) - prefix matching (legacy)",
          "Bash(npm run *) - wildcard matching"}};
    if(content==":*")
      return {false,"Prefix cannot be empty before :*",
        "Specify a command prefix before :*",
        std::vector<std::string>{"Bash(npm:*)","Bash(git:*)"}};
  }
  if(isFilePatternGadget(parsed.gadgetName,services)&&parsed.ruleContent){
    const auto& content=*parsed.ruleContent;
    if(has(content,":*"))
      return {false,"The \":*\" syntax is only for Bash prefix rules",
        "Use glob patterns like \"*\" or \"**\" for file matching",
        std::vector<std::string>{parsed.gadgetName+"(*.ts) - matches .ts files",
          parsed.gadgetName+"(src/**) - matches all files in src",
          parsed.gadgetName+"(**/*.test.ts) - matches test files"}};
    static const std::regex acceptable(R"(^\*|\*$|\*\*|/\*|\*\.|\*\))");
    if(has(content,"*")&&!std::regex_search(content,acceptable)&&
       !has(content,"**"))
      return {false,"Wildcard placement might be incorrect",
        "Wildcards are typically used at path boundaries",
        std::vector<std::string>{parsed.gadgetName+"(*.js) - all .js files",
          parsed.gadgetName+"(src/*) - all files directly in src",
          parsed.gadgetName+"(src/**) - all files recursively in src"}};
  }
  return {true};
}
Json validationResultJson(const RuleValidationResult& result){
  Json value={{"valid",result.valid}};
  if(result.error)value["error"]=*result.error;
  if(result.suggestion)value["suggestion"]=*result.suggestion;
  if(result.examples)value["examples"]=*result.examples;
  return value;
}
}
