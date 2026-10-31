#include "bootstrap/Costs.hpp"
#include <stdexcept>

namespace batbox::bootstrap {
CostState initialCostState(const std::function<double()>& nowMs){
  if(!nowMs)throw std::invalid_argument("Missing bootstrap clock");
  CostState state;state.startTime=nowMs();return state;
}
void addToTotalDurationState(CostState& s,double d,double withoutRetries){
  s.totalAPIDuration+=d;s.totalAPIDurationWithoutRetries+=withoutRetries;
}
void resetTotalDurationStateAndCostForTests(CostState& s){
  s.totalAPIDuration=0;s.totalAPIDurationWithoutRetries=0;s.totalCostUSD=0;
}
void addToTotalCostState(CostState& s,double cost,const Json& usage,
    const std::string& model){s.modelUsage[model]=usage;s.totalCostUSD+=cost;}
double getTotalCostUSD(const CostState& s){return s.totalCostUSD;}
double getTotalAPIDuration(const CostState& s){return s.totalAPIDuration;}
double getTotalDuration(const CostState& s,
    const std::function<double()>& nowMs){
  if(!nowMs)throw std::invalid_argument("Missing bootstrap clock");
  return nowMs()-s.startTime;
}
double getTotalAPIDurationWithoutRetries(const CostState& s){
  return s.totalAPIDurationWithoutRetries;}
double getTotalGadgetDuration(const CostState& s){return s.totalGadgetDuration;}
void addToGadgetDuration(CostState& s,double d){
  s.totalGadgetDuration+=d;s.turnGadgetDurationMs+=d;s.turnGadgetCount++;
}
double getTurnHookDurationMs(const CostState& s){return s.turnHookDurationMs;}
void addToTurnHookDuration(CostState& s,double d){
  s.turnHookDurationMs+=d;s.turnHookCount++;
}
void resetTurnHookDuration(CostState& s){
  s.turnHookDurationMs=0;s.turnHookCount=0;
}
double getTurnHookCount(const CostState& s){return s.turnHookCount;}
double getTurnGadgetDurationMs(const CostState& s){return s.turnGadgetDurationMs;}
void resetTurnGadgetDuration(CostState& s){
  s.turnGadgetDurationMs=0;s.turnGadgetCount=0;
}
double getTurnGadgetCount(const CostState& s){return s.turnGadgetCount;}
double getTurnClassifierDurationMs(const CostState& s){
  return s.turnClassifierDurationMs;}
void addToTurnClassifierDuration(CostState& s,double d){
  s.turnClassifierDurationMs+=d;s.turnClassifierCount++;
}
void resetTurnClassifierDuration(CostState& s){
  s.turnClassifierDurationMs=0;s.turnClassifierCount=0;
}
double getTurnClassifierCount(const CostState& s){
  return s.turnClassifierCount;}
std::shared_ptr<StatsStore> getStatsStore(const CostState& s){
  return s.statsStore;}
void setStatsStore(CostState& s,std::shared_ptr<StatsStore> store){
  s.statsStore=std::move(store);
}
void addToTotalLinesChanged(CostState& s,double added,double removed){
  s.totalLinesAdded+=added;s.totalLinesRemoved+=removed;
}
double getTotalLinesAdded(const CostState& s){return s.totalLinesAdded;}
double getTotalLinesRemoved(const CostState& s){return s.totalLinesRemoved;}
void setHasUnknownModelCost(CostState& s){s.hasUnknownModelCost=true;}
bool hasUnknownModelCost(const CostState& s){return s.hasUnknownModelCost;}
std::optional<double> getLastApiCompletionTimestamp(const CostState& s){
  return s.lastApiCompletionTimestamp;
}
void setLastApiCompletionTimestamp(CostState& s,double timestamp){
  s.lastApiCompletionTimestamp=timestamp;
}
}
