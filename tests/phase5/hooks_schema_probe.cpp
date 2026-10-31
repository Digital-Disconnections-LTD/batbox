#include "instructions/hooks/runtime/HookSchema.hpp"
#include "instructions/hooks/runtime/PermissionUpdate.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
int main(int argc,char** argv){
  if(argc!=2)throw std::invalid_argument("cases path required");
  std::ifstream stream(argv[1]);Json cases;stream>>cases;Json rows=Json::array();
  hook::HookSchemaImports imports;
  imports.parsePermissionUpdate=hook::parseHookPermissionUpdate;
  for(const auto& item:cases){
    const auto result=hook::hookOutputSchemaSafeParse(item.at("value"),imports);
    Json row={{"name",item.at("name")},{"success",result.json.has_value()}};
    if(result.json)row["data"]=*result.json;
    else{
      Json issues=Json::array();
      for(const auto& issue:result.issues)
        issues.push_back({{"path",issue.path},{"message",issue.message}});
      row["issues"]=std::move(issues);
    }
    rows.push_back(std::move(row));
  }
  std::cout<<value::stringify(rows)<<'\n';
}
