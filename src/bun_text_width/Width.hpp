#pragma once
#include <cstddef>
#include <string_view>

namespace batbox::bun_text_width {
std::size_t widthUtf16(std::u16string_view text,bool countAnsiEscapeCodes=false,
  bool ambiguousIsNarrow=true);
std::size_t widthLatin1(std::u16string_view text,bool countAnsiEscapeCodes=false);
} // namespace batbox::bun_text_width
