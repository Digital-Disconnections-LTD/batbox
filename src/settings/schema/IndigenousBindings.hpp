#pragma once
#include "settings/schema/Main.hpp"
#include "settings/schema/imports/Bindings.hpp"
#include "runtime/Url.hpp"

namespace batbox::settings::schema {
inline std::function<SchemaResult(const Json&)> bindIndigenousSettingsSchema(
  const MainConfig& config,const PermissionRuleServices& rules){
  return imports::bindSettingsSchema(config,rules,runtime::canonicalUrl);
}
}
