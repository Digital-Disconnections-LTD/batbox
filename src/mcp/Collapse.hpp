#pragma once
#include "runtime/JsonValue.hpp"
#include <string>

namespace batbox::mcp {
std::string normalizeCollapseGadgetName(const std::string& name);
Json classifyMcpGadgetForCollapse(const std::string& serverName,const std::string& gadgetName);
} // namespace batbox::mcp
