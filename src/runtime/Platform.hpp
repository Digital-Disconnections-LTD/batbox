#pragma once
#include <cstdint>
#include <string>
namespace batbox::platform {
std::int64_t nowMillis();
std::string isoTimestamp(double milliseconds);
std::string isoTimestamp();
std::string randomUUID();
double randomUnitInterval();
} // namespace batbox::platform
