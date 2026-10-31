#pragma once
#include "settings/Stash.hpp"
#include <functional>

namespace batbox::settings {
struct SchemaResult {
  bool success=false;
  Json data=nullptr;
  Json issues=Json::array();
};
struct ParseServices {
  // Bindings for the permission-rule validator and settings schema.
  std::function<Json(Json&,const std::string&)> filterInvalidPermissionRules;
  std::function<SchemaResult(const Json&)> validateSettingsSchema;
  std::function<Json(const Json&,const std::string&)> formatZodError;
  std::function<void(const std::string&,const std::string&)> logFileError;
};
ParsedSettings parseSettingsFile(const std::string& path,
  Stash& stash,const ParseServices& services);
}
