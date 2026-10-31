#include "interactive/cards/BashResultText.hpp"
#include "glob/Regex16.hpp"
#include "runtime/JsonValue.hpp"

namespace batbox::interactive::cards {
namespace {
const glob::Regex16& violations() {
  static const glob::Regex16 pattern(
      u"<sandbox_violations>([\\s\\S]*?)<\\/sandbox_violations>");
  return pattern;
}
const glob::Regex16& cwdWarningPattern() {
  // The warning only matches at the absolute end. Dot excludes line
  // terminators, including U+2028 and U+2029.
  static const glob::Regex16 pattern(
      u"(?:^|\\n)(Shell cwd was reset to [^\\n\\r\\x{2028}\\x{2029}]+)"
      u"$");
  return pattern;
}
} // namespace

std::string removeSandboxViolationTags(const std::string& text) {
  const auto units = glob::units(text);
  return glob::text(violations().replace(units,
      [](const glob::RegexMatch&) { return std::u16string{}; }, true));
}

std::string stderrWithoutSandboxViolations(const std::string& stderrText) {
  if (!violations().search(glob::units(stderrText))) return stderrText;
  return value::trim(removeSandboxViolationTags(stderrText));
}

CwdResetStderr extractCwdResetWarning(const std::string& stderrText) {
  const auto units = glob::units(stderrText);
  glob::RegexMatch match;
  if (!cwdWarningPattern().search(units, 0, &match))
    return {stderrText, std::nullopt};
  auto cleaned = units;
  cleaned.erase(match.start, match.end - match.start);
  return {value::trim(glob::text(cleaned)),
          match.groups.size() > 1 && match.groups[1] ?
            std::optional<std::string>{glob::text(*match.groups[1])} :
            std::nullopt};
}
} // namespace batbox::interactive::cards
