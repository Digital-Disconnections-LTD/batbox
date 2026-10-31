#include "interactive/cards/OutputLineCard.hpp"
#include "glob/Regex16.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>
#include <string_view>
#include <vector>

namespace batbox::interactive::cards {
namespace {
bool space(char16_t c) {
  return (c >= 9 && c <= 13) || c == 0x20 || c == 0xa0 || c == 0x1680 ||
    (c >= 0x2000 && c <= 0x200a) || c == 0x2028 || c == 0x2029 ||
    c == 0x202f || c == 0x205f || c == 0x3000 || c == 0xfeff;
}
std::string withoutWhitespace(const std::string& value) {
  std::u16string result;
  for (const auto unit : value::utf16CodeUnits(value))
    if (!space(unit)) result.push_back(unit);
  return value::fromUtf16(result);
}
std::vector<std::string> splitLines(const std::string& content) {
  std::vector<std::string> lines;
  std::size_t start = 0;
  while (start <= content.size()) {
    const auto end = content.find('\n', start);
    lines.push_back(content.substr(start, end - start));
    if (end == std::string::npos) break;
    start = end + 1;
  }
  return lines;
}
} // namespace

std::string tryFormatJson(const std::string& line) {
  try {
    const auto parsed = value::parse(line);
    const auto stringified = value::stringify(parsed);
    std::string normalizedOriginal;
    normalizedOriginal.reserve(line.size());
    for (std::size_t i = 0; i < line.size(); ++i) {
      if (line[i] == '\\' && i + 1 < line.size() && line[i + 1] == '/') {
        normalizedOriginal.push_back('/');
        ++i;
      } else normalizedOriginal.push_back(line[i]);
    }
    if (withoutWhitespace(normalizedOriginal) !=
        withoutWhitespace(stringified)) return line;
    return value::stringify(parsed, 2);
  } catch (...) {
    return line;
  }
}

std::string tryJsonFormatContent(const std::string& content) {
  if (value::utf16CodeUnits(content).size() > 10000) return content;
  std::string result;
  const auto lines = splitLines(content);
  for (std::size_t i = 0; i < lines.size(); ++i) {
    if (i) result.push_back('\n');
    result += tryFormatJson(lines[i]);
  }
  return result;
}

std::string linkifyUrlsInText(const std::string& content,
                              const OutputLineImports& imports) {
  if (!imports.createHyperlink)
    throw std::logic_error("Hyperlink service required");
  static const glob::Regex16 pattern(u"https?:\\/\\/[^\\s\"'<>\\\\]+", false);
  const auto units = glob::units(content);
  return glob::text(pattern.replace(units,
      [&](const glob::RegexMatch& match) {
        return glob::units(imports.createHyperlink(
            glob::text(*match.groups.front())));
      }, true));
}

std::string stripUnderlineAnsi(const std::string& content) {
  static const glob::Regex16 pattern(
      u"\\x1b\\[([0-9]+;)*4(;[0-9]+)*m|\\x1b\\[4(;[0-9]+)*m|"
      u"\\x1b\\[([0-9]+;)*4m");
  const auto units = glob::units(content);
  return glob::text(pattern.replace(units,
      [](const glob::RegexMatch&) { return std::u16string{}; }, true));
}
} // namespace batbox::interactive::cards
