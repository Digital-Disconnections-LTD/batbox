#pragma once

#include <functional>
#include <optional>
#include <string>

namespace batbox::interactive::cards_assistant {

struct UpsellParams {
  bool shouldShowUpsell = false;
  bool isMax20x = false;
  bool isExtraUsageCommandEnabled = false;
  bool shouldAutoOpenRateLimitOptionsMenu = false;
  bool isTeamOrEnterprise = false;
  bool hasBillingAccess = false;
};

std::optional<std::string> getUpsellMessage(const UpsellParams& params);

struct RateLimitInput {
  std::string text;
  std::string subscriptionType;
  std::string rateLimitTier;
  bool isSubscriber = false;
  std::string limitsStatus;
  bool hasResetTime = false;
  bool isUsingOverage = false;
  bool extraUsageEnabled = false;
  bool hasBillingAccess = false;
  bool canOpenRateLimitOptions = false;
};

struct RateLimitProtocol {
  std::string max20xTier;
};

struct RateLimitCard {
  std::string text;
  std::optional<std::string> upsell;
  bool shouldAutoOpenOptions = false;
};

// One retained display instance. Call committed() after the view is presented.
struct RateLimitState {
  bool initialized = false;
  bool stashedUpsell = false;
  bool hasOpenedInteractiveMenu = false;
  bool isTeamOrEnterprise = false;
  bool isMax20x = false;
  bool shouldShowUpsell = false;
  bool stashedAutoOpen = false;
  std::optional<std::string> upsell;

  RateLimitCard render(const RateLimitInput& input,
                       const RateLimitProtocol& protocol);
  void committed(const RateLimitCard& card,
                 const std::function<void()>& openRateLimitOptions);
};

} // namespace batbox::interactive::cards_assistant
