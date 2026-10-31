#include "bootstrap/Costs.hpp"
#include <cmath>
#include <limits>

namespace batbox::bootstrap {
namespace {
double sumUsage(const CostState& state,const char* key){
  double total=0;
  if(!state.modelUsage.is_object())return total;
  for(auto it=state.modelUsage.begin();it!=state.modelUsage.end();++it){
    const auto& usage=it.value();
    if(!usage.is_object()||!usage.contains(key))continue;
    const auto& value=usage[key];
    if(value.is_number())total+=value.get<double>();
    else if(value.is_null())total+=0;
    else total=std::numeric_limits<double>::quiet_NaN();
  }
  return total;
}
}
const Json& getModelUsage(const CostState& s){return s.modelUsage;}
std::optional<Json> getUsageForModel(const CostState& s,
    const std::string& model){
  if(!s.modelUsage.is_object()||!s.modelUsage.contains(model))
    return std::nullopt;
  return s.modelUsage[model];
}
double getTotalInputTokens(const CostState& s){
  return sumUsage(s,"inputTokens");}
double getTotalOutputTokens(const CostState& s){
  return sumUsage(s,"outputTokens");}
double getTotalStashReadInputTokens(const CostState& s){
  return sumUsage(s,"cacheReadInputTokens");}
double getTotalStashCreationInputTokens(const CostState& s){
  return sumUsage(s,"cacheCreationInputTokens");}
double getTotalWebSearchRequests(const CostState& s){
  return sumUsage(s,"webSearchRequests");}
double getTurnOutputTokens(const CostState& s){
  return getTotalOutputTokens(s)-s.outputTokensAtTurnStart;
}
std::optional<double> getCurrentTurnTokenBudget(const CostState& s){
  return s.currentTurnTokenBudget;
}
void snapshotOutputTokensForTurn(CostState& s,std::optional<double> budget){
  s.outputTokensAtTurnStart=getTotalOutputTokens(s);
  s.currentTurnTokenBudget=budget;
  s.budgetContinuationCount=0;
}
double getBudgetContinuationCount(const CostState& s){
  return s.budgetContinuationCount;
}
void incrementBudgetContinuationCount(CostState& s){
  s.budgetContinuationCount++;
}
}
