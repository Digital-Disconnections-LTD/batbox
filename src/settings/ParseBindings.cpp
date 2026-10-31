#include "settings/ParseBindings.hpp"
#include "settings/Validation.hpp"

namespace batbox::settings {
ParseServices bindParser(
  std::function<SchemaResult(const Json&)> validateSettingsSchema,
  const PermissionRuleServices& rules,
  const std::string& docsBase,
  std::function<void(const std::string&,const std::string&)> logFileError){
  ParseServices services;
  services.filterInvalidPermissionRules=[rules](Json& data,
    const std::string& path){
    return filterInvalidPermissionRules(data,path,rules);
  };
  services.validateSettingsSchema=std::move(validateSettingsSchema);
  services.formatZodError=[docsBase](const Json& issues,
    const std::string& path){return formatZodError(issues,path,docsBase);};
  services.logFileError=std::move(logFileError);
  return services;
}
}
