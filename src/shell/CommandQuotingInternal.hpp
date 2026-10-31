#pragma once
#include <string>

namespace batbox::shell_command::detail {
bool whitespace(char16_t c);
bool digit(char16_t c);
bool word(char16_t c);
bool newline(char16_t c);
bool multilineQuoted(const std::u16string& code,char16_t quote);
} // namespace batbox::shell_command::detail
