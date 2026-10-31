#include "query/TokenBudget.hpp"
#include <cmath>
#include <stdexcept>

namespace batbox::query {
namespace {
constexpr double COMPLETION_THRESHOLD = 0.9;
constexpr double DIMINISHING_THRESHOLD = 500;
}

BudgetTracker createBudgetTracker(const BudgetDependencies& deps) {
  if (!deps.nowMillis) throw std::invalid_argument("Budget clock dependency is required");
  BudgetTracker tracker;
  tracker.startedAt = deps.nowMillis();
  return tracker;
}

Json checkTokenBudget(BudgetTracker& tracker, const std::optional<std::string>& batId,
                      const std::optional<double>& budget, double globalTurnTokens,
                      const BudgetDependencies& deps) {
  if ((batId && !batId->empty()) || !budget || *budget <= 0)
    return Json{{"action", "stop"}, {"completionEvent", nullptr}};

  const double turnTokens = globalTurnTokens;
  const double pct = std::floor((turnTokens / *budget) * 100 + 0.5);
  const double deltaSinceLastCheck = globalTurnTokens - tracker.lastGlobalTurnTokens;
  const bool isDiminishing = tracker.continuationCount >= 3 &&
    deltaSinceLastCheck < DIMINISHING_THRESHOLD &&
    tracker.lastDeltaTokens < DIMINISHING_THRESHOLD;

  if (!isDiminishing && turnTokens < *budget * COMPLETION_THRESHOLD) {
    if (!deps.continuationMessage)
      throw std::invalid_argument("Budget continuation message dependency is required");
    ++tracker.continuationCount;
    tracker.lastDeltaTokens = deltaSinceLastCheck;
    tracker.lastGlobalTurnTokens = globalTurnTokens;
    return Json{{"action", "continue"},
      {"nudgeMessage", deps.continuationMessage(pct, turnTokens, *budget)},
      {"continuationCount", tracker.continuationCount}, {"pct", pct},
      {"turnTokens", turnTokens}, {"budget", *budget}};
  }

  if (isDiminishing || tracker.continuationCount > 0) {
    if (!deps.nowMillis) throw std::invalid_argument("Budget clock dependency is required");
    return Json{{"action", "stop"}, {"completionEvent", Json{
      {"continuationCount", tracker.continuationCount}, {"pct", pct},
      {"turnTokens", turnTokens}, {"budget", *budget},
      {"diminishingReturns", isDiminishing},
      {"durationMs", deps.nowMillis() - tracker.startedAt}}}};
  }
  return Json{{"action", "stop"}, {"completionEvent", nullptr}};
}

} // namespace batbox::query
