#pragma once
#include "runtime/JsonValue.hpp"
#include "text_width/WrapAnsi.hpp"
#include <optional>
#include <string>

namespace batbox::bun_text_width {
// Pinned Bun 1.3.11 JS binding, including input/column/options conversion.
std::string wrapAnsiJs(const std::optional<Json>& input,
  const std::optional<Json>& columns,const std::optional<Json>& options);
// Typed adapter for text_width::WrapAnsiServices.resolveBunWrapAnsi.
text_width::WrapAnsiRef indigenousWrapAnsiRef();
} // namespace batbox::bun_text_width
