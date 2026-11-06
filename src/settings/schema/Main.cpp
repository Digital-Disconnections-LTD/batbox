#include "settings/schema/Main.hpp"
#include "settings/schema/Descriptor.hpp"
#include "settings/schema/Runtime.hpp"

namespace batbox::settings::schema {
SchemaResult settings(const Json& input,const MainConfig& config,
  const MainImports& imports){
  const Json node=settingsDescriptor().at("base");
  auto parsed=detail::parseNode(node,input,true,Json::array(),config,imports);
  return {parsed.valid,parsed.valid?parsed.value:Json(nullptr),parsed.issues};
}
}
