#pragma once
#include "runtime/JsonValue.hpp"
#include "text_width/StringWidth.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace batbox::bun_text_width {
// Bun 1.3.11 SDK binding. A missing value represents JavaScript undefined.
double stringWidth(const std::optional<Json>& value,const Json& options=Json());
std::size_t widthUtf16(std::u16string_view text,bool countAnsiEscapeCodes=false,
  bool ambiguousIsNarrow=true);
std::size_t widthLatin1(std::u16string_view text,bool countAnsiEscapeCodes=false);
// Supply as Services.resolveBunStringWidth = [] { return indigenousWidthCallback(); }.
text_width::BunWidth indigenousWidthCallback();
} // namespace batbox::bun_text_width
