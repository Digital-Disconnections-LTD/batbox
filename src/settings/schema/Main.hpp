#pragma once
#include "settings/schema/Leaves.hpp"
#include <functional>
#include <optional>

namespace batbox::settings::schema {
using UrlCanonicalizer=std::function<std::optional<std::string>(
  const std::string&)>;
struct MainConfig {
  std::string schemaUrl="batbox://settings-schema";
};
struct MainImports {
  PermissionRuleServices permissionRules;
  std::function<SchemaResult(const Json&)> hooks;
  UrlCanonicalizer canonicalUrl;
};
SchemaResult settings(const Json& input,const MainConfig& config,
  const MainImports& imports);
}
