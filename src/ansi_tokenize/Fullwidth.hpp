#pragma once
#include <cstdint>
namespace batbox::ansi_tokenize {
bool isFullwidthCodePoint(std::uint32_t codePoint);
}
