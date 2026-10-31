#pragma once
#include <string_view>
#include <vector>

namespace batbox::mcp {
const std::vector<std::string_view>& collapseSearchNames();
const std::vector<std::string_view>& collapseRead1Names();
const std::vector<std::string_view>& collapseRead2Names();
const std::vector<std::string_view>& collapseRead3Names();
} // namespace batbox::mcp
