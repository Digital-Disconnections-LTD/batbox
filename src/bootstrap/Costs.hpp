#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace batbox::bootstrap {
struct StatsStore {
  virtual ~StatsStore()=default;
  virtual void observe(const std::string& name,double value)=0;
};
struct CostState {
  double totalCostUSD=0,totalAPIDuration=0,
    totalAPIDurationWithoutRetries=0,totalGadgetDuration=0,
    turnHookDurationMs=0,turnGadgetDurationMs=0,
    turnClassifierDurationMs=0,startTime=0,
    totalLinesAdded=0,totalLinesRemoved=0;
  double turnGadgetCount=0,turnHookCount=0,turnClassifierCount=0;
  bool hasUnknownModelCost=false;
  Json modelUsage=Json::object();
  double outputTokensAtTurnStart=0,budgetContinuationCount=0;
  std::optional<double> currentTurnTokenBudget;
  std::optional<double> lastApiCompletionTimestamp;
  std::shared_ptr<StatsStore> statsStore;
};
struct RestoreCosts {
  double totalCostUSD=0,totalAPIDuration=0,
    totalAPIDurationWithoutRetries=0,totalGadgetDuration=0,
    totalLinesAdded=0,totalLinesRemoved=0;
  std::optional<double> lastDuration;
  std::optional<Json> modelUsage;
};
CostState initialCostState(const std::function<double()>& nowMs);
void addToTotalDurationState(CostState& state,double duration,
  double durationWithoutRetries);
void resetTotalDurationStateAndCostForTests(CostState& state);
void addToTotalCostState(CostState& state,double cost,const Json& modelUsage,
  const std::string& model);
double getTotalCostUSD(const CostState& state);
double getTotalAPIDuration(const CostState& state);
double getTotalDuration(const CostState& state,
  const std::function<double()>& nowMs);
double getTotalAPIDurationWithoutRetries(const CostState& state);
double getTotalGadgetDuration(const CostState& state);
void addToGadgetDuration(CostState& state,double duration);
double getTurnHookDurationMs(const CostState& state);
void addToTurnHookDuration(CostState& state,double duration);
void resetTurnHookDuration(CostState& state);
double getTurnHookCount(const CostState& state);
double getTurnGadgetDurationMs(const CostState& state);
void resetTurnGadgetDuration(CostState& state);
double getTurnGadgetCount(const CostState& state);
double getTurnClassifierDurationMs(const CostState& state);
void addToTurnClassifierDuration(CostState& state,double duration);
void resetTurnClassifierDuration(CostState& state);
double getTurnClassifierCount(const CostState& state);
std::shared_ptr<StatsStore> getStatsStore(const CostState& state);
void setStatsStore(CostState& state,std::shared_ptr<StatsStore> store);
void addToTotalLinesChanged(CostState& state,double added,double removed);
double getTotalLinesAdded(const CostState& state);
double getTotalLinesRemoved(const CostState& state);
const Json& getModelUsage(const CostState& state);
std::optional<Json> getUsageForModel(const CostState& state,
  const std::string& model);
double getTotalInputTokens(const CostState& state);
double getTotalOutputTokens(const CostState& state);
double getTotalStashReadInputTokens(const CostState& state);
double getTotalStashCreationInputTokens(const CostState& state);
double getTotalWebSearchRequests(const CostState& state);
double getTurnOutputTokens(const CostState& state);
std::optional<double> getCurrentTurnTokenBudget(const CostState& state);
void snapshotOutputTokensForTurn(CostState& state,
  std::optional<double> budget);
double getBudgetContinuationCount(const CostState& state);
void incrementBudgetContinuationCount(CostState& state);
void setHasUnknownModelCost(CostState& state);
bool hasUnknownModelCost(const CostState& state);
std::optional<double> getLastApiCompletionTimestamp(
  const CostState& state);
void setLastApiCompletionTimestamp(CostState& state,double timestamp);
void resetCostState(CostState& state,const std::function<double()>& nowMs);
void setCostStateForRestore(CostState& state,const RestoreCosts& values,
  const std::function<double()>& nowMs);
}
