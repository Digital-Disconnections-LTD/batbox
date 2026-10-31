#pragma once
#include <string>

namespace batbox::app_state {
std::string toExternalPermissionMode(const std::string& mode);
std::string permissionModeFromString(const std::string& value,
  bool transcriptClassifierEnabled);
} // namespace batbox::app_state
