#pragma once
#include <string>

namespace batbox::runtime {
// The version this binary was built as (the CMake project version), for
// User-Agent headers, MCP clientInfo and Flight records.
const char* batboxVersion();
// "batbox/<version>", the User-Agent Batbox sends unless one is configured.
std::string userAgent();
} // namespace batbox::runtime
