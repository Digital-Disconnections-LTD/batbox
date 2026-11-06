#pragma once
#include "settings/PermissionRules.hpp"

namespace batbox::settings {
struct SchemaResult {
  bool success=false;
  Json data=nullptr;
  Json issues=Json::array();
};
}
namespace batbox::settings::schema {
SchemaResult environmentVariables(const Json& input);
SchemaResult permissions(const Json& input,const PermissionRuleServices& rules);
}
