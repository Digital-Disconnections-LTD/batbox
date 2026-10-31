#include "bootstrap/Costs.hpp"
#include <stdexcept>

namespace batbox::bootstrap {
void resetCostState(CostState& s,const std::function<double()>& nowMs){
  if(!nowMs)throw std::invalid_argument("Missing bootstrap clock");
  s.totalCostUSD=0;
  s.totalAPIDuration=0;
  s.totalAPIDurationWithoutRetries=0;
  s.totalGadgetDuration=0;
  s.startTime=nowMs();
  s.totalLinesAdded=0;
  s.totalLinesRemoved=0;
  s.hasUnknownModelCost=false;
  s.modelUsage=Json::object();
}
void setCostStateForRestore(CostState& s,const RestoreCosts& values,
    const std::function<double()>& nowMs){
  s.totalCostUSD=values.totalCostUSD;
  s.totalAPIDuration=values.totalAPIDuration;
  s.totalAPIDurationWithoutRetries=values.totalAPIDurationWithoutRetries;
  s.totalGadgetDuration=values.totalGadgetDuration;
  s.totalLinesAdded=values.totalLinesAdded;
  s.totalLinesRemoved=values.totalLinesRemoved;
  if(values.modelUsage)s.modelUsage=*values.modelUsage;
  if(values.lastDuration&&*values.lastDuration!=0){
    if(!nowMs)throw std::invalid_argument("Missing bootstrap clock");
    s.startTime=nowMs()-*values.lastDuration;
  }
}
}
