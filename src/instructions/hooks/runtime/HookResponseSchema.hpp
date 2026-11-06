#pragma once
#include "instructions/hooks/runtime/PoetryHookExecutor.hpp"

namespace batbox::instructions::hooks::runtime {
// Validate hook responses and strip unknown fields. Validation failures use
// Zod v4-compatible error.message JSON text.
HookResponseParse hookResponseSchemaSafeParse(const Json& input);
} // namespace batbox::instructions::hooks::runtime
