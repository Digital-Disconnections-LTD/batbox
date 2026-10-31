#include "interactive/cards_system/SystemAPIErrorCard.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace batbox::interactive::cards_system {

APIErrorCard APIErrorState::render(const APIErrorInput& input) const {
  APIErrorCard card;
  card.visible = !(input.retryAttempt < 4);
  if (!card.visible) return card;
  const auto units = value::utf16CodeUnits(input.formattedError);
  card.truncated = !input.verbose && units.size() > 1000;
  card.errorText = card.truncated ?
      value::fromUtf16(units.substr(0, 1000)) + "…" :
      input.formattedError;
  const double seconds = std::max(0.0,
      std::floor((input.retryInMs - countdownMs) / 1000 + 0.5));
  const auto renderedSeconds = value::numberToString(seconds);
  card.retryText = "Retrying in " + renderedSeconds +
      (seconds == 1 ? " second… (attempt " : " seconds… (attempt ") +
      value::numberToString(input.retryAttempt) + "/" +
      value::numberToString(input.maxRetries) + ")";
  if (input.apiTimeoutEnvironment && !input.apiTimeoutEnvironment->empty())
    card.retryText += " · API_TIMEOUT_MS=" + *input.apiTimeoutEnvironment +
                      "ms, try increasing it";
  card.intervalActive = countdownMs < input.retryInMs;
  return card;
}

void APIErrorState::intervalTick(const APIErrorCard& current) {
  if (!current.intervalActive)
    throw std::logic_error("Inactive API error interval cannot tick");
  countdownMs += 1000;
}
} // namespace batbox::interactive::cards_system
