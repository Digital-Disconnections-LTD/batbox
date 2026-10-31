#pragma once
#include "settings/schema/Main.hpp"

namespace batbox::settings::schema::imports {
MainImports bindMainImports(const PermissionRuleServices& rules,
  UrlCanonicalizer canonicalUrl);
std::function<SchemaResult(const Json&)> bindSettingsSchema(
  const MainConfig& config,const PermissionRuleServices& rules,
  UrlCanonicalizer canonicalUrl);
}
