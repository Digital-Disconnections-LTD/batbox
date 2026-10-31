#pragma once
#include "runtime/JsonValue.hpp"
#include <string>

namespace batbox::shell_command {
bool hasMalformedTokens(const std::string& command,const Json& parsed);
bool hasShellQuoteSingleQuoteBug(const std::string& command);
} // namespace batbox::shell_command
