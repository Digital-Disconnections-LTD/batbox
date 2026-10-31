#pragma once
#include "ansi_tokenize/Types.hpp"
#include <optional>
#include <string>

namespace batbox::terminal_screen {
std::optional<std::string> extractHyperlinkFromStyles(
  const ansi_tokenize::Codes& styles);
ansi_tokenize::Codes filterOutHyperlinkStyles(
  const ansi_tokenize::Codes& styles);
} // namespace batbox::terminal_screen
