#pragma once
#include "settings/Parse.hpp"
#include "settings/PermissionRules.hpp"

namespace batbox::settings {
ParseServices bindParser(
  std::function<SchemaResult(const Json&)> validateSettingsSchema,
  const PermissionRuleServices& rules,
  const std::string& docsBase,
  std::function<void(const std::string&,const std::string&)> logFileError={});
}
