#pragma once
#include "cli/structured/ControlAdapters.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::cli::structured {
struct IndigenousSchemaServices {
  // Log malformed optional permission updates, then omit the field.
  std::function<void(const std::string&)> logMalformedPermissionUpdates;
};
std::optional<Json> parsePermissionUpdate(const Json& value);
Json parsePermissionOutput(const Json& value,
  const IndigenousSchemaServices& services);
Json parseElicitationResponse(const Json& value);
Json parseHookOutput(const Json& value);
Json parseMcpResponse(const Json& value);
ControlSchemas makeIndigenousControlSchemas(IndigenousSchemaServices services);
}
