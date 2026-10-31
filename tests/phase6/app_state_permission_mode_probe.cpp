#include "app_state/PermissionModeBridge.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using namespace batbox;
int main(){
  Json rows=Json::array();
  for(bool enabled:{false,true}){
    Json values=Json::array();
    for(const std::string value:{"default","plan","acceptEdits",
        "bypassPermissions","dontAsk","auto","bubble","garbage",""})
      values.push_back({{"value",value},
        {"parsed",app_state::permissionModeFromString(value,enabled)},
        {"external",app_state::toExternalPermissionMode(value)}});
    rows.push_back({{"enabled",enabled},{"values",values}});
  }
  std::cout<<rows.dump()<<'\n';
}
