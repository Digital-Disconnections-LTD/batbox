#pragma once
#include "runtime/JsonValue.hpp"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace batbox::npm_wrap_ansi {
struct Options { bool trim=true,hard=false,wordWrap=true; };
using Rows=std::vector<std::u16string>;
std::u16string normalize(std::u16string_view input);
std::u16string trimStart(std::u16string_view text);
bool blank(std::u16string_view text);
double width(std::u16string_view text);
std::u16string stripAnsi(std::u16string_view text);
std::u16string trimVisibleSpacesRight(std::u16string_view text);
void wrapWord(Rows& rows,std::u16string_view word,double columns);
std::u16string wrapLine(std::u16string_view line,double columns,const Options& options);
std::u16string preserveAnsi(std::u16string_view rows);
const std::string& emojiPattern();
const std::string& ansiPattern();
double eastAsianWidth(std::uint32_t cp);
}
