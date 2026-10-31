#pragma once
#include "runtime/JsonValue.hpp"

namespace batbox::instructions::hooks::runtime {
// Validated data with unknown fields stripped; nullopt indicates validation failure.
std::optional<Json> parseHookPermissionUpdate(const Json& input);
} // namespace batbox::instructions::hooks::runtime
