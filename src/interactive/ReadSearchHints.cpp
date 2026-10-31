#include "interactive/ReadSearchCollapse.hpp"
#include "runtime/JsonValue.hpp"

namespace batbox::interactive {
namespace {
bool space(char16_t c) {
  return (c >= 9 && c <= 13) || c == 0x20 || c == 0xa0 || c == 0x1680 ||
    (c >= 0x2000 && c <= 0x200a) || c == 0x2028 || c == 0x2029 ||
    c == 0x202f || c == 0x205f || c == 0x3000 || c == 0xfeff;
}
} // namespace

std::string commandAsHint(const std::string& command) {
  std::u16string cleaned = u"$ ";
  const auto source = value::utf16CodeUnits(command);
  std::size_t start = 0;
  bool first = true;
  while (start <= source.size()) {
    const auto end = source.find(u'\n', start);
    const auto line = std::u16string_view(source).substr(start, end - start);
    std::u16string collapsed;
    bool inWhitespace = false;
    for (const auto c : line) {
      if (space(c)) {
        if (!inWhitespace) collapsed.push_back(u' ');
        inWhitespace = true;
      } else {
        collapsed.push_back(c);
        inWhitespace = false;
      }
    }
    const auto trimmed = value::utf16CodeUnits(value::trim(value::fromUtf16(collapsed)));
    if (!trimmed.empty()) {
      if (!first) cleaned.push_back(u'\n');
      cleaned += trimmed;
      first = false;
    }
    if (end == std::u16string::npos) break;
    start = end + 1;
  }
  if (cleaned.size() > 300) return value::fromUtf16(cleaned.substr(0, 299) + u"…");
  return value::fromUtf16(cleaned);
}
} // namespace batbox::interactive
