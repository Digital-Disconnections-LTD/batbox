#pragma once
#include <cstdint>
#include <string>
namespace batbox::platform {
std::int64_t nowMillis();
std::string isoTimestamp(double milliseconds);
std::string isoTimestamp();
std::string randomUUID();
double randomUnitInterval();
// Return freed allocator pages to the operating system where the platform
// allocator supports it (after compaction and at the end of a turn).
void releaseFreeMemory();
} // namespace batbox::platform
