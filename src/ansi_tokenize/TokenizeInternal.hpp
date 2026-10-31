#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace batbox::ansi_tokenize {
std::optional<std::u16string> parseLinkCode(std::u16string_view text,std::size_t offset);
std::optional<std::u16string> parseOscSequence(std::u16string_view text,std::size_t offset);
std::optional<std::u16string> parseSgrSequence(std::u16string_view text,std::size_t offset);
std::vector<std::u16string> splitCompoundSgr(std::u16string_view code);
} // namespace batbox::ansi_tokenize
