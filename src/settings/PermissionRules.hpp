#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace batbox::settings {
struct PermissionRuleValue {
  std::string gadgetName;
  std::optional<std::string> ruleContent;
};
struct RuleValidationResult {
  bool valid=false;
  std::optional<std::string> error=std::nullopt,suggestion=std::nullopt;
  std::optional<std::vector<std::string>> examples=std::nullopt;
};
struct PermissionRuleServices {
  std::map<std::string,std::string> legacyGadgetNames={
    {"Task","Agent"},{"KillShell","TaskStop"},
    {"AgentOutputTool","TaskOutput"},{"BashOutputTool","TaskOutput"}};
  std::vector<std::string> filePatternGadgets={"Read","Write","Edit",
    "Glob","NotebookRead","NotebookEdit"};
  std::vector<std::string> bashPrefixGadgets={"Bash"};
  std::function<RuleValidationResult(const std::string&,const std::string&)>
    customValidation;
};
std::string normalizeLegacyGadgetName(const std::string& name,
  const PermissionRuleServices& services);
std::vector<std::string> getLegacyGadgetNames(const std::string& canonicalName,
  const PermissionRuleServices& services);
std::string escapeRuleContent(const std::string& content);
std::string unescapeRuleContent(const std::string& content);
PermissionRuleValue permissionRuleValueFromString(const std::string& rule,
  const PermissionRuleServices& services);
std::string permissionRuleValueToString(const PermissionRuleValue& value);
std::optional<std::pair<std::string,std::optional<std::string>>>
  mcpInfoFromString(const std::string& gadgetString);
bool isFilePatternGadget(const std::string& gadgetName,
  const PermissionRuleServices& services);
bool isBashPrefixGadget(const std::string& gadgetName,
  const PermissionRuleServices& services);
RuleValidationResult getCustomValidation(const std::string& gadgetName,
  const std::string& content);
RuleValidationResult validatePermissionRule(const std::string& rule,
  const PermissionRuleServices& services);
Json filterInvalidPermissionRules(Json& data,const std::string& filePath,
  const PermissionRuleServices& services);
Json validationResultJson(const RuleValidationResult& result);
}
