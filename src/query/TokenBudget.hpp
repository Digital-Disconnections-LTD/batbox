#pragma once
#include "runtime/JsonValue.hpp"
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace batbox::query {

struct BudgetTracker {
  int continuationCount = 0;
  double lastDeltaTokens = 0;
  double lastGlobalTurnTokens = 0;
  std::int64_t startedAt = 0;
};

struct BudgetDependencies {
  std::function<std::int64_t()> nowMillis;
  std::function<std::string(double pct, double turnTokens, double budget)> continuationMessage;
};

BudgetTracker createBudgetTracker(const BudgetDependencies& deps);
Json checkTokenBudget(BudgetTracker& tracker, const std::optional<std::string>& batId,
                      const std::optional<double>& budget, double globalTurnTokens,
                      const BudgetDependencies& deps);

} // namespace batbox::query
