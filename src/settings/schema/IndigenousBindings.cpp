#include "settings/schema/IndigenousBindings.hpp"
#include "settings/schema/imports/Bindings.hpp"
#include "runtime/Url.hpp"

namespace batbox::settings::schema {
std::function<SchemaResult(const Json&)> bindIndigenousSettingsSchema(
    const MainConfig& config,const PermissionRuleServices& rules){
  return imports::bindSettingsSchema(config,rules,runtime::canonicalUrl);
}
}
