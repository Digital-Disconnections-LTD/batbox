#include "settings/PermissionRules.hpp"
#include <algorithm>

namespace batbox::settings {
bool isFilePatternGadget(const std::string& gadgetName,
  const PermissionRuleServices& services){
  return std::find(services.filePatternGadgets.begin(),
    services.filePatternGadgets.end(),gadgetName)!=services.filePatternGadgets.end();
}
bool isBashPrefixGadget(const std::string& gadgetName,
  const PermissionRuleServices& services){
  return std::find(services.bashPrefixGadgets.begin(),
    services.bashPrefixGadgets.end(),gadgetName)!=services.bashPrefixGadgets.end();
}
RuleValidationResult getCustomValidation(const std::string& gadgetName,
  const std::string& content){
  if(gadgetName=="WebSearch"){
    if(content.find('*')!=std::string::npos||
       content.find('?')!=std::string::npos)
      return {false,"WebSearch does not support wildcards",
        "Use exact search terms without * or ?",
        std::vector<std::string>{"WebSearch(batbox ai)",
          "WebSearch(typescript tutorial)"}};
  }else if(gadgetName=="WebFetch"){
    const auto examples=std::vector<std::string>{
      "WebFetch(domain:example.com)","WebFetch(domain:github.com)"};
    if(content.find("://")!=std::string::npos||content.starts_with("http"))
      return {false,"WebFetch permissions use domain format, not URLs",
        "Use \"domain:hostname\" format",examples};
    if(!content.starts_with("domain:"))
      return {false,"WebFetch permissions must use \"domain:\" prefix",
        "Use \"domain:hostname\" format",
        std::vector<std::string>{"WebFetch(domain:example.com)",
          "WebFetch(domain:*.google.com)"}};
  }
  return {true,std::nullopt,std::nullopt,std::nullopt};
}
}
