#pragma once
#include <cstdint>
#include <string>
namespace batbox::text_width::data {
const std::string& emojiPattern();
const std::string& ansiPattern();
double eastAsianWidth(std::uint32_t cp);
}
