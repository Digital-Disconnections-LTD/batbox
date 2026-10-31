#include "interactive/MessageTags.hpp"
#include "glob/Regex16.hpp"
#include "runtime/JsonValue.hpp"

namespace batbox::interactive {
std::string escapeRegExp(const std::string& value) {
  std::string escaped;
  for (const char c : value) {
    if (c == '.' || c == '*' || c == '+' || c == '?' || c == '^' ||
        c == '$' || c == '{' || c == '}' || c == '(' || c == ')' ||
        c == '|' || c == '[' || c == ']' || c == '\\') escaped.push_back('\\');
    escaped.push_back(c);
  }
  return escaped;
}

std::optional<std::string> extractTag(const std::string& html,
                                      const std::string& tagName) {
  if (value::trim(html).empty() || value::trim(tagName).empty()) return {};
  const auto escapedTag = glob::units(escapeRegExp(tagName));
  const auto text = glob::units(html);
  const glob::Regex16 pattern(u"<" + escapedTag + u"(?:\\s+[^>]*)?>" +
      u"([\\s\\S]*?)<\\/" + escapedTag + u">", true);
  const glob::Regex16 opening(u"<" + escapedTag + u"(?:\\s+[^>]*?)?>", true);
  const glob::Regex16 closing(u"<\\/" + escapedTag + u">", true);
  std::size_t from = 0, lastIndex = 0;
  glob::RegexMatch match, nested;
  while (pattern.search(text, from, &match)) {
    const auto before = std::u16string_view(text).substr(lastIndex, match.start - lastIndex);
    int depth = 0;
    std::size_t at = 0;
    while (opening.search(before, at, &nested)) {
      ++depth;
      at = nested.end;
      if (at >= before.size()) break;
    }
    at = 0;
    while (closing.search(before, at, &nested)) {
      --depth;
      at = nested.end;
      if (at >= before.size()) break;
    }
    if (depth == 0 && match.groups.size() > 1 && match.groups[1] &&
        !match.groups[1]->empty()) return glob::text(*match.groups[1]);
    lastIndex = match.end;
    from = match.end;
    if (from >= text.size()) break;
  }
  return {};
}
} // namespace batbox::interactive
