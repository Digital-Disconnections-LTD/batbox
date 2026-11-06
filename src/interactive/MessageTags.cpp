#include "interactive/MessageTags.hpp"
#include "glob/Regex16.hpp"
#include "runtime/JsonValue.hpp"
#include <cctype>
#include <memory>
#include <string_view>
#include <unordered_map>

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

namespace {
// A match needs "</tag>" (case folded). Plain ASCII that never closes the tag
// cannot match; anything non-ASCII near a "</" is left to the regex, since
// Unicode case folding can turn it into the tag's letters.
bool mayCloseTag(std::string_view text, std::string_view tag) {
  for (auto at = text.find("</"); at != std::string_view::npos;
       at = text.find("</", at + 2)) {
    const auto rest = text.substr(at + 2);
    bool same = rest.size() > tag.size();
    for (std::size_t i = 0; same && i <= tag.size() && i < rest.size(); ++i) {
      const auto c = static_cast<unsigned char>(rest[i]);
      if (c >= 0x80) return true;
      if (i == tag.size()) same = c == '>';
      else same = std::tolower(c) == std::tolower(static_cast<unsigned char>(tag[i]));
    }
    if (same) return true;
  }
  return false;
}
struct TagPatterns {
  glob::Regex16 pattern, opening, closing;
};
const TagPatterns& tagPatterns(const std::string& tagName) {
  // A handful of fixed tag names; compile each once per thread.
  thread_local std::unordered_map<std::string, std::unique_ptr<TagPatterns>> compiled;
  auto& slot = compiled[tagName];
  if (!slot) {
    const auto escapedTag = glob::units(escapeRegExp(tagName));
    slot = std::make_unique<TagPatterns>(TagPatterns{
      glob::Regex16(u"<" + escapedTag + u"(?:\\s+[^>]*)?>" +
        u"([\\s\\S]*?)<\\/" + escapedTag + u">", true),
      glob::Regex16(u"<" + escapedTag + u"(?:\\s+[^>]*?)?>", true),
      glob::Regex16(u"<\\/" + escapedTag + u">", true)});
  }
  return *slot;
}
} // namespace

std::optional<std::string> extractTag(const std::string& html,
                                      const std::string& tagName) {
  if (value::trimView(html).empty() || value::trimView(tagName).empty()) return {};
  if (!mayCloseTag(html, tagName)) return {};
  const auto text = glob::units(html);
  const auto& [pattern, opening, closing] = tagPatterns(tagName);
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
