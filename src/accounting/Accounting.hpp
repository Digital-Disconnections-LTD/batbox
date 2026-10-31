#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <string>

namespace batbox::accounting {
struct AccountingState {
  double totalCostUSD=0;
  Json modelUsage=Json::object();
  bool hasUnknownModelCost=false;
};
struct CostServices {
  Json modelCosts=Json::object();             // keyed by canonical provider model
  Json premiumCurrentStandardCosts;           // Standard premium tier.
  Json premiumCurrentFastCosts;               // Fast premium tier.
  Json unknownFallbackCosts;                  // Configured fallback tier.
  std::string premiumCurrentCanonical;
  std::function<std::string(const std::string&)> getCanonicalName;
  std::function<std::string()> getDefaultMainLoopModelSetting;
  std::function<bool()> isFastModeEnabled;
  std::function<Json()> getSdkBetas;
  std::function<double(const std::string&,const Json&)> getContextWindowForModel;
  std::function<Json(const std::string&)> getModelMaxOutputTokens;
};
Json getPremiumCurrentCostTier(bool fastMode,const CostServices& services);
Json getModelCosts(const std::string& model,const Json& usage,
  AccountingState& state,const CostServices& services);
double tokensToUSDCost(const Json& modelCosts,const Json& usage);
double calculateUSDCost(const std::string& resolvedModel,const Json& usage,
  AccountingState& state,const CostServices& services);
double calculateCostFromTokens(const std::string& model,const Json& tokens,
  AccountingState& state,const CostServices& services);
Json getAdvisorUsage(const Json& usage);
Json addToTotalModelUsage(double cost,const Json& usage,const std::string& model,
  AccountingState& state,const CostServices& services);
void addToTotalCostState(double cost,const Json& modelUsage,const std::string& model,
  AccountingState& state);
double addToTotalFlightCost(double cost,const Json& usage,const std::string& model,
  AccountingState& state,const CostServices& services);
} // namespace batbox::accounting
