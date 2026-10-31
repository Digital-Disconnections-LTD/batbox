#include "interactive/cards/UserPoetryLeaf.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::interactive::cards {
namespace {
constexpr std::size_t maxDisplayChars = 10000;
constexpr std::size_t headChars = 2500;
constexpr std::size_t tailChars = 2500;

std::ptrdiff_t countNewlines(std::u16string_view text, std::size_t start = 0) {
  std::ptrdiff_t count = 0;
  for (std::size_t i = start; i < text.size(); ++i)
    if (text[i] == u'\n') ++count;
  return count;
}
} // namespace

std::string poetryDisplayText(const std::string& text) {
  const auto units = value::utf16CodeUnits(text);
  if (units.size() <= maxDisplayChars) return text;
  const auto head = units.substr(0, headChars);
  const auto tail = units.substr(units.size() - tailChars);
  const auto hiddenLines = countNewlines(units, headChars) - countNewlines(tail);
  return value::fromUtf16(head) + "\n… +" + std::to_string(hiddenLines) +
         " lines …\n" + value::fromUtf16(tail);
}

std::optional<PoetryLeaf> poetryLeaf(const std::string& text,
                                     const std::optional<std::string>& timestamp,
                                     bool addMargin,
                                     const PoetryEnvironment& environment) {
  if (!environment.logError) throw std::logic_error("poetry diagnostics required");
  bool briefLayout = false;
  if (environment.briefFeatureEnabled) {
    if (!environment.briefModeActive || !environment.userMessageOptIn ||
        !environment.briefPreference)
      throw std::logic_error("Brief poetry configuration required");
    const bool active = environment.briefModeActive() ||
        (environment.userMessageOptIn() &&
         (environment.briefEnvironmentEnabled || environment.briefPreference()));
    briefLayout = active && environment.isBriefOnly &&
        !environment.isTranscriptMode && !environment.viewingBatMission;
  }
  auto display = poetryDisplayText(text);
  if (text.empty()) {
    environment.logError("No content found in user poetry message");
    return {};
  }
  return PoetryLeaf{
    std::move(display), briefLayout, briefLayout ? timestamp : std::nullopt,
    environment.selected ? PoetryBackground::Selected :
      briefLayout ? PoetryBackground::None : PoetryBackground::User,
    addMargin, briefLayout ? 0 : 1
  };
}
} // namespace batbox::interactive::cards
