#pragma once
#include "settings/schema/Leaves.hpp"
#include <functional>
#include <optional>

namespace batbox::settings::schema {
using UrlCanonicalizer=std::function<std::optional<std::string>(
  const std::string&)>;
struct MainConfig {
  bool xaa=false;
  bool transcriptClassifier=false;
  bool lodestone=false;
  bool proactive=false;
  bool kairos=false;
  bool voiceMode=false;
  bool kairosBrief=false;
  bool internal=false;
  std::string schemaUrl="batbox://settings-schema";
};
struct MainImports {
  PermissionRuleServices permissionRules;
  std::function<SchemaResult(const Json&)> hooks;
  std::function<SchemaResult(const Json&)> sandbox;
  std::function<SchemaResult(const Json&)> marketplaceSource;
  UrlCanonicalizer canonicalUrl;
};
SchemaResult settings(const Json& input,const MainConfig& config,
  const MainImports& imports);
}
