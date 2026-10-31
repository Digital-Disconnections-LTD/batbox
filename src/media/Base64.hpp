#pragma once
#include <string>
#include <string_view>
namespace batbox::media::indigenous {
std::string base64Encode(const std::string& bytes);
std::string base64Decode(std::string_view encoded);
}
