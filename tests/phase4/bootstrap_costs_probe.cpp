#include "bootstrap/Costs.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>

namespace {
struct Stats:batbox::bootstrap::StatsStore {
  void observe(const std::string&,double)override{}
};
}
int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    Json trace=Json::array();
    auto state=bootstrap::initialCostState([]{return 1000.0;});
    auto now=[&]{trace.push_back("now");return 2000.0;};
    if(test.contains("ops"))for(const auto& op:test["ops"]){
      const auto kind=op.value("kind",std::string());
      if(kind=="duration")bootstrap::addToTotalDurationState(state,
        op["value"].get<double>(),op["without"].get<double>());
      else if(kind=="resetDuration")
        bootstrap::resetTotalDurationStateAndCostForTests(state);
      else if(kind=="cost")bootstrap::addToTotalCostState(state,
        op["value"].get<double>(),op["usage"],
        op["model"].get<std::string>());
      else if(kind=="tool")bootstrap::addToGadgetDuration(state,
        op["value"].get<double>());
      else if(kind=="hook")bootstrap::addToTurnHookDuration(state,
        op["value"].get<double>());
      else if(kind=="classifier")
        bootstrap::addToTurnClassifierDuration(state,
          op["value"].get<double>());
      else if(kind=="resetHook")bootstrap::resetTurnHookDuration(state);
      else if(kind=="resetTool")bootstrap::resetTurnGadgetDuration(state);
      else if(kind=="resetClassifier")
        bootstrap::resetTurnClassifierDuration(state);
      else if(kind=="lines")bootstrap::addToTotalLinesChanged(state,
        op["added"].get<double>(),op["removed"].get<double>());
      else if(kind=="snapshot"){
        std::optional<double> budget;
        if(op.contains("budget")&&!op["budget"].is_null())
          budget=op["budget"].get<double>();
        bootstrap::snapshotOutputTokensForTurn(state,budget);
      }else if(kind=="continuation")
        bootstrap::incrementBudgetContinuationCount(state);
      else if(kind=="unknown")bootstrap::setHasUnknownModelCost(state);
      else if(kind=="completion")
        bootstrap::setLastApiCompletionTimestamp(state,
          op["value"].get<double>());
      else if(kind=="resetCost")bootstrap::resetCostState(state,now);
      else if(kind=="restore"){
        const auto& values=op["values"];
        bootstrap::RestoreCosts restore;
        restore.totalCostUSD=values.value("totalCostUSD",0.0);
        restore.totalAPIDuration=values.value("totalAPIDuration",0.0);
        restore.totalAPIDurationWithoutRetries=values.value(
          "totalAPIDurationWithoutRetries",0.0);
        restore.totalGadgetDuration=values.value("totalToolDuration",0.0);
        restore.totalLinesAdded=values.value("totalLinesAdded",0.0);
        restore.totalLinesRemoved=values.value("totalLinesRemoved",0.0);
        if(values.contains("lastDuration")&&
            !values["lastDuration"].is_null())
          restore.lastDuration=values["lastDuration"].get<double>();
        if(values.contains("modelUsage")&&
            !values["modelUsage"].is_null())
          restore.modelUsage=values["modelUsage"];
        bootstrap::setCostStateForRestore(state,restore,now);
      }else if(kind=="stats")bootstrap::setStatsStore(state,
        std::make_shared<Stats>());
      else throw std::runtime_error("Unknown cost operation");
    }
    const auto number=[](const std::optional<double>& value)->Json{
      return value?Json(*value):Json(nullptr);};
    const Json result={{"cost",bootstrap::getTotalCostUSD(state)},
      {"api",bootstrap::getTotalAPIDuration(state)},
      {"duration",bootstrap::getTotalDuration(state,now)},
      {"apiWithout",bootstrap::getTotalAPIDurationWithoutRetries(state)},
      {"tool",bootstrap::getTotalGadgetDuration(state)},
      {"turnHook",bootstrap::getTurnHookDurationMs(state)},
      {"hookCount",bootstrap::getTurnHookCount(state)},
      {"turnTool",bootstrap::getTurnGadgetDurationMs(state)},
      {"toolCount",bootstrap::getTurnGadgetCount(state)},
      {"turnClassifier",bootstrap::getTurnClassifierDurationMs(state)},
      {"classifierCount",bootstrap::getTurnClassifierCount(state)},
      {"linesAdded",bootstrap::getTotalLinesAdded(state)},
      {"linesRemoved",bootstrap::getTotalLinesRemoved(state)},
      {"input",bootstrap::getTotalInputTokens(state)},
      {"output",bootstrap::getTotalOutputTokens(state)},
      {"cacheRead",bootstrap::getTotalStashReadInputTokens(state)},
      {"cacheCreate",bootstrap::getTotalStashCreationInputTokens(state)},
      {"web",bootstrap::getTotalWebSearchRequests(state)},
      {"turnOutput",bootstrap::getTurnOutputTokens(state)},
      {"budget",number(bootstrap::getCurrentTurnTokenBudget(state))},
      {"continuations",bootstrap::getBudgetContinuationCount(state)},
      {"unknown",bootstrap::hasUnknownModelCost(state)},
      {"completion",number(bootstrap::getLastApiCompletionTimestamp(state))},
      {"usage",bootstrap::getModelUsage(state)},
      {"usageA",bootstrap::getUsageForModel(state,"model-a")
        .value_or(Json(nullptr))},
      {"hasStats",static_cast<bool>(bootstrap::getStatsStore(state))},
      {"trace",trace}};
    std::cout<<value::stringify(result)<<'\n';
  }
}
