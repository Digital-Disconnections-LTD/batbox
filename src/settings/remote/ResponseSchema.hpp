#pragma once
#include "runtime/JsonValue.hpp"
#include <string>

namespace batbox::settings::remote {
struct ResponseParseResult {
  bool success=false;
  Json data=nullptr;
  std::string errorMessage;
};
ResponseParseResult parseRemoteManagedSettingsResponse(const Json& input);
}
