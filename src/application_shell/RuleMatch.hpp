#pragma once
#include "runtime/JsonValue.hpp"
#include <optional>
#include <string>

namespace batbox::application_shell {
struct ShellRule {
  enum class Type { Exact, Prefix, Wildcard } type=Type::Exact;
  std::string content;
};
std::optional<std::string> permissionRuleExtractPrefix(const std::string& rule);
bool hasWildcards(const std::string& pattern);
bool matchWildcardPattern(const std::string& pattern,const std::string& command,
  bool caseInsensitive=false);
ShellRule parsePermissionRule(const std::string& rule);
} // namespace batbox::application_shell
