#pragma once
#include <string>

namespace batbox::app_state {
std::string permissionModeFromString(const std::string& value,
  bool transcriptClassifierEnabled);
} // namespace batbox::app_state
