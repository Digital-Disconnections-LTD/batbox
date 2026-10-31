#pragma once
#include <string>
#include <string_view>
namespace batbox::mcp {
std::string decodeBase64(std::string_view encoded);
std::string encodeBase64(std::string_view bytes);
} // namespace batbox::mcp
