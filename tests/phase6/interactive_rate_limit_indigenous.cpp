#include "interactive/cards_assistant/RateLimitCard.hpp"
#include <cassert>
#include <iostream>

using namespace batbox::interactive::cards_assistant;

int main() {
  RateLimitState state;
  RateLimitInput input;
  input.text = "limited";
  input.subscriptionType = "team";
  input.isSubscriber = true;
  input.limitsStatus = "rejected";
  input.hasResetTime = true;
  input.canOpenRateLimitOptions = true;
  input.extraUsageEnabled = true;
  const RateLimitProtocol protocol{"tier20x"};
  const auto first = state.render(input, protocol);
  assert(first.shouldAutoOpenOptions);
  assert(first.upsell == "Opening your options…");
  int opens = 0;
  state.committed(first, [&] { ++opens; });
  assert(opens == 1);
  const auto afterEffect = state.render(input, protocol);
  assert(!afterEffect.shouldAutoOpenOptions && !afterEffect.upsell);
  state.committed(afterEffect, [&] { ++opens; });
  assert(opens == 1);

  RateLimitState stashed;
  input.canOpenRateLimitOptions = false;
  input.hasBillingAccess = true;
  const auto before = stashed.render(input, protocol);
  assert(before.upsell ==
         "/extra-usage to finish what you’re working on.");
  input.extraUsageEnabled = false;
  input.hasBillingAccess = false;
  const auto retained = stashed.render(input, protocol);
  assert(retained.upsell == before.upsell);
  input.limitsStatus = "approved";
  const auto noReset = stashed.render(input, protocol);
  assert(noReset.upsell == before.upsell);

  RateLimitState max;
  input.rateLimitTier = "tier20x";
  const auto maxView = max.render(input, protocol);
  assert(maxView.upsell ==
         "/login to switch to an API usage-billed account.");
  std::cout << "4 native rate-limit state transitions pass\n";
}
