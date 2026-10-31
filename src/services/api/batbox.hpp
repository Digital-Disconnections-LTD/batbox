#pragma once
#include "runtime/JsonValue.hpp"
namespace batbox {
extern const Json EMPTY_USAGE;
Json updateUsage(const Json& usage, const Json& partUsage, bool stashedMicrocompact = false);
Json accumulateUsage(const Json& totalUsage, const Json& messageUsage, bool stashedMicrocompact = false);
} // namespace batbox
