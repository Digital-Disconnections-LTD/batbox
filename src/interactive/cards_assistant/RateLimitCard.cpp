#include "interactive/cards_assistant/RateLimitCard.hpp"
#include <stdexcept>

namespace batbox::interactive::cards_assistant {

std::optional<std::string> getUpsellMessage(const UpsellParams& p) {
  if (!p.shouldShowUpsell) return {};
  if (p.isMax20x) {
    if (p.isExtraUsageCommandEnabled)
      return "/extra-usage to finish what you’re working on.";
    return "/login to switch to an API usage-billed account.";
  }
  if (p.shouldAutoOpenRateLimitOptionsMenu)
    return "Opening your options…";
  if (!p.isTeamOrEnterprise && !p.isExtraUsageCommandEnabled)
    return "/upgrade to increase your usage limit.";
  if (p.isTeamOrEnterprise) {
    if (!p.isExtraUsageCommandEnabled) return {};
    if (p.hasBillingAccess)
      return "/extra-usage to finish what you’re working on.";
    return "/extra-usage to request more usage from your admin.";
  }
  return "/upgrade or /extra-usage to finish what you’re working on.";
}

RateLimitCard RateLimitState::render(const RateLimitInput& input,
                                    const RateLimitProtocol& protocol) {
  if (!initialized) {
    isTeamOrEnterprise = input.subscriptionType == "team" ||
                         input.subscriptionType == "enterprise";
    isMax20x = input.rateLimitTier == protocol.max20xTier;
    shouldShowUpsell = input.isSubscriber;
    initialized = true;
  }
  const bool canSeeOptions = shouldShowUpsell && !isMax20x;
  const bool currentlyLimited = input.limitsStatus == "rejected" &&
      input.hasResetTime && !input.isUsingOverage;
  const bool autoOpen = canSeeOptions && !hasOpenedInteractiveMenu &&
      currentlyLimited && input.canOpenRateLimitOptions;
  if (!stashedUpsell || stashedAutoOpen != autoOpen) {
    upsell = getUpsellMessage({shouldShowUpsell, isMax20x,
      input.extraUsageEnabled, autoOpen, isTeamOrEnterprise,
      input.hasBillingAccess});
    stashedAutoOpen = autoOpen;
    stashedUpsell = true;
  }
  return {input.text, hasOpenedInteractiveMenu ? std::nullopt : upsell,
          autoOpen};
}

void RateLimitState::committed(
    const RateLimitCard& card,
    const std::function<void()>& openRateLimitOptions) {
  if (!card.shouldAutoOpenOptions) return;
  if (!openRateLimitOptions)
    throw std::logic_error("Rate limit options action required");
  hasOpenedInteractiveMenu = true;
  openRateLimitOptions();
}

} // namespace batbox::interactive::cards_assistant
