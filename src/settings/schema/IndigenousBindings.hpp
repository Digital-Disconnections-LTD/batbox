#pragma once
#include "settings/schema/Main.hpp"
#include "settings/Parse.hpp"

namespace batbox::settings::schema {
std::function<SchemaResult(const Json&)> bindIndigenousSettingsSchema(
  const MainConfig& config,const PermissionRuleServices& rules);
ParseServices bindIndigenousSettingsParser(const MainConfig& config,
  const PermissionRuleServices& rules,const std::string& docsBase,
  std::function<void(const std::string&,const std::string&)> logFileError={});
}
