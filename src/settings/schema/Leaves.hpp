#pragma once
#include "settings/Parse.hpp"
#include "settings/PermissionRules.hpp"

namespace batbox::settings::schema {
struct Config {bool transcriptClassifier=false;};
SchemaResult environmentVariables(const Json& input);
SchemaResult permissions(const Json& input,const Config& config,
  const PermissionRuleServices& rules);
SchemaResult allowedMcpServerEntry(const Json& input);
SchemaResult deniedMcpServerEntry(const Json& input);
}
