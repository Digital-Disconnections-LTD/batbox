#pragma once
#include <cstdint>
namespace batbox::text_sdk::ansi_tokenize {
bool isFullwidthCodePoint(std::uint32_t codePoint);
}
