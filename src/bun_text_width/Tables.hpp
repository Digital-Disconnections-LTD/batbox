#pragma once
#include <cstdint>

namespace batbox::bun_text_width {
bool isFullWidth(std::uint32_t cp);
bool isAmbiguous(std::uint32_t cp);
bool hasEmojiProperty(std::uint32_t cp);
// GraphemeBreakNoControl enum value from the pinned Bun grapheme.zig.
std::uint8_t graphemeProperty(std::uint32_t cp);
} // namespace batbox::bun_text_width
