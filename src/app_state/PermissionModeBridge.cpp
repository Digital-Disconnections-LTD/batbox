#include "app_state/PermissionModeBridge.hpp"

namespace batbox::app_state {
std::string permissionModeFromString(const std::string& value,
  bool transcriptClassifierEnabled){
  if(value=="acceptEdits"||value=="bypassPermissions"||
    value=="default"||value=="dontAsk"||value=="plan"||
    (transcriptClassifierEnabled&&value=="auto"))return value;
  return "default";
}
} // namespace batbox::app_state
