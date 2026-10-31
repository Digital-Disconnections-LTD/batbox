#pragma once
#include "runtime/JsonValue.hpp"
#include "gadgets/Gadget.hpp"
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace batbox::skills {
using RuleEntries=std::vector<std::pair<std::string,Json>>;
struct SkillGadgetLookup {
  std::function<Json(const std::string&,const std::vector<Json>&)> findCommand;
};
std::vector<Json> getAllSkillCommands(const std::vector<Json>& localCommands,
  const std::vector<Json>& mcpCommands);
gadgets::ValidationResult validateSkillInput(const std::string& skill,
  const std::vector<Json>& commands,const SkillGadgetLookup& lookup);
bool skillRuleMatches(const std::string& ruleContent,const std::string& commandName);
bool skillHasOnlySafeProperties(const Json& command);
Json checkSkillPermissions(const Json& input,const std::vector<Json>& commands,
  const RuleEntries& denyRules,const RuleEntries& allowRules,
  const SkillGadgetLookup& lookup,const std::string& gadgetName="Skill",
  const std::function<bool(const std::string&)>& isRemoteSkill={});
Json checkSkillPermissions(const Json& input,const std::vector<Json>& commands,
  const RuleEntries& denyRules,const std::function<RuleEntries()>& loadAllowRules,
  const SkillGadgetLookup& lookup,const std::string& gadgetName,
  const std::function<bool(const std::string&)>& isRemoteSkill);
}
