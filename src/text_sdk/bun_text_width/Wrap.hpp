#pragma once
#include "runtime/JsonValue.hpp"
#include "text_sdk/width/WrapAnsi.hpp"
#include <optional>
#include <string>

namespace batbox::text_sdk::bun_text_width {
// Pinned Bun 1.3.11 JS binding, including input/column/options conversion.
std::string wrapAnsiJs(const std::optional<Json>& input,
  const std::optional<Json>& columns,const std::optional<Json>& options);
// Typed adapter for text_sdk::width::WrapAnsiServices.resolveBunWrapAnsi.
text_sdk::width::WrapAnsiRef indigenousWrapAnsiRef();
} // namespace batbox::text_sdk::bun_text_width
