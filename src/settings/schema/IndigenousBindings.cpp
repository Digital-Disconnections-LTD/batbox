#include "settings/schema/IndigenousBindings.hpp"
#include "settings/schema/imports/Bindings.hpp"
#include "settings/ParseBindings.hpp"
#include "runtime/Url.hpp"

namespace batbox::settings::schema {
std::function<SchemaResult(const Json&)> bindIndigenousSettingsSchema(
    const MainConfig& config,const PermissionRuleServices& rules){
  return imports::bindSettingsSchema(config,rules,runtime::canonicalUrl);
}
ParseServices bindIndigenousSettingsParser(const MainConfig& config,
    const PermissionRuleServices& rules,const std::string& docsBase,
    std::function<void(const std::string&,const std::string&)> logFileError){
  return bindParser(bindIndigenousSettingsSchema(config,rules),rules,
    docsBase,std::move(logFileError));
}
}
