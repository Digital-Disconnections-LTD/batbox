#pragma once
#include "settings/schema/Main.hpp"

namespace batbox::settings::schema::detail {
struct ParseResult {
  bool valid=false;
  bool present=true;
  Json value=nullptr;
  Json issues=Json::array();
};
ParseResult parseNode(const Json& node,const Json& input,bool present,
  const Json& path,const MainConfig& config,const MainImports& imports);
ParseResult parseScalar(const Json& node,const Json& input,bool present,
  const Json& path,const MainConfig& config,const MainImports& imports);
Json fieldsForConfig(const Json& descriptor,const MainConfig& config);
void appendIssues(Json& target,const Json& source,const Json& prefix);
}
