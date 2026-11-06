#pragma once
#include "settings/schema/Main.hpp"

namespace batbox::settings::schema {
std::function<SchemaResult(const Json&)> bindIndigenousSettingsSchema(
  const MainConfig& config,const PermissionRuleServices& rules);
}
