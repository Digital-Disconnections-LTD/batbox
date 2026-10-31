#include "interactive/cards/BashResultCard.hpp"
#include "interactive/cards/BashResultText.hpp"
#include "runtime/JsonValue.hpp"
#include <cmath>

namespace batbox::interactive::cards {
namespace {
std::string stringOrEmpty(const Json& object, const char* key) {
  const auto found = object.find(key);
  return found != object.end() && found->is_string() ?
      found->get<std::string>() : "";
}
} // namespace

BashResultCard buildBashResultCard(const Json& content,
                                   bool verbose,
                                   std::optional<double> timeoutMs) {
  const auto stdoutText = stringOrEmpty(content, "stdout");
  const auto stderrWithViolations = stringOrEmpty(content, "stderr");
  const auto withoutViolations = stderrWithoutSandboxViolations(stderrWithViolations);
  const auto extracted = extractCwdResetWarning(withoutViolations);
  BashResultCard card;
  card.verbose = verbose;
  card.image = value::truthy(value::property(content, "isImage"));
  if (card.image) return card;
  if (!stdoutText.empty()) card.stdoutText = stdoutText;
  if (!value::trim(extracted.cleanedStderr).empty())
    card.stderrText = extracted.cleanedStderr;
  card.cwdResetWarning = extracted.cwdResetWarning;
  if (stdoutText.empty() && value::trim(extracted.cleanedStderr).empty() &&
      !card.cwdResetWarning) {
    if (value::truthy(value::property(content, "backgroundTaskId"))) {
      card.noOutputText = "Running in the background";
      card.backgroundHint = true;
    } else if (value::truthy(value::property(content, "returnCodeInterpretation"))) {
      card.noOutputText = stringOrEmpty(content, "returnCodeInterpretation");
    } else {
      card.noOutputText = value::truthy(value::property(content, "noOutputExpected")) ?
          "Done" : "(No output)";
    }
  }
  if (timeoutMs && *timeoutMs != 0 && !std::isnan(*timeoutMs))
    card.timeoutMs = timeoutMs;
  return card;
}
} // namespace batbox::interactive::cards
