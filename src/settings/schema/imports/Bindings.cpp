#include "settings/schema/imports/Bindings.hpp"
#include "settings/schema/imports/Hooks.hpp"
#include "settings/schema/imports/Marketplace.hpp"
#include "settings/schema/imports/Sandbox.hpp"
#include <stdexcept>

namespace batbox::settings::schema::imports {
MainImports bindMainImports(const PermissionRuleServices& rules,
  UrlCanonicalizer canonicalUrl){
  if(!canonicalUrl)
    throw std::invalid_argument("Missing native settings URL parser");
  MainImports imports;
  imports.permissionRules=rules;
  imports.canonicalUrl=canonicalUrl;
  imports.hooks=[canonicalUrl](const Json& input){
    return hooks(input,canonicalUrl);};
  imports.sandbox=[](const Json& input){return sandboxSettings(input);};
  imports.marketplaceSource=[canonicalUrl](const Json& input){
    return marketplaceSource(input,canonicalUrl);};
  return imports;
}
std::function<SchemaResult(const Json&)> bindSettingsSchema(
  const MainConfig& config,const PermissionRuleServices& rules,
  UrlCanonicalizer canonicalUrl){
  const auto imports=bindMainImports(rules,std::move(canonicalUrl));
  return [config,imports](const Json& input){
    return settings(input,config,imports);};
}
}
