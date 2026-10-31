#pragma once
#include <cstdint>
#include <optional>

namespace batbox::terminal_output {
std::uint32_t bidiCharType(char16_t character);
std::optional<std::uint32_t> openingToClosingBracket(
  std::optional<std::uint32_t> character);
std::optional<std::uint32_t> closingToOpeningBracket(
  std::optional<std::uint32_t> character);
std::optional<std::uint32_t> canonicalBracket(
  std::optional<std::uint32_t> character);
} // namespace batbox::terminal_output
