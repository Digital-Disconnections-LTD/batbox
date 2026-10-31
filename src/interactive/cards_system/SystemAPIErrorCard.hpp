#pragma once

#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::interactive::cards_system {

struct APIErrorInput {
  double retryAttempt = 0;
  double retryInMs = 0;
  double maxRetries = 0;
  std::string formattedError;
  bool verbose = false;
  std::optional<std::string> apiTimeoutEnvironment;
};

struct APIErrorCard {
  bool visible = false;
  bool truncated = false;
  bool intervalActive = false;
  std::string errorText;
  std::string retryText;
};

struct APIErrorState {
  double countdownMs = 0;
  APIErrorCard render(const APIErrorInput& input) const;
  void intervalTick(const APIErrorCard& current);
};

// Uses the existing indigenous API error formatter at the actual message boundary.
APIErrorInput apiErrorInputFromMessage(
    const Json& message, bool verbose,
    std::optional<std::string> apiTimeoutEnvironment);

} // namespace batbox::interactive::cards_system
