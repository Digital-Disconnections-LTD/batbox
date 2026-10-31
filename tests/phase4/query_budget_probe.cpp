#include "query/TokenBudget.hpp"
#include <iostream>

using batbox::Json;

int main() {
  std::string line;
  while (std::getline(std::cin,line)) {
    try {
      const Json input=batbox::value::parse(line);
      std::int64_t now=1000;
      batbox::query::BudgetDependencies deps;
      deps.nowMillis=[&]{return now;};
      deps.continuationMessage=[](double pct,double tokens,double budget) {
        return "continue:"+batbox::value::stringify(Json(pct))+":"+
          batbox::value::stringify(Json(tokens))+":"+batbox::value::stringify(Json(budget));
      };
      auto tracker=batbox::query::createBudgetTracker(deps);
      Json decisions=Json::array();
      for(const Json& step:input.at("steps")) {
        now=step.value("now",std::int64_t{1000});
        std::optional<std::string> bat;
        if(step.contains("agentId") && step["agentId"].is_string())bat=step["agentId"].get<std::string>();
        std::optional<double> budget;
        if(step.contains("budget")&&!step["budget"].is_null())budget=step["budget"].get<double>();
        decisions.push_back(batbox::query::checkTokenBudget(tracker,bat,budget,step.at("tokens").get<double>(),deps));
      }
      std::cout<<batbox::value::stringify(Json{{"decisions",decisions},{"tracker",Json{
        {"continuationCount",tracker.continuationCount},{"lastDeltaTokens",tracker.lastDeltaTokens},
        {"lastGlobalTurnTokens",tracker.lastGlobalTurnTokens},{"startedAt",tracker.startedAt}}}})<<'\n';
    }catch(const std::exception& e){
      std::cout<<batbox::value::stringify(Json{{"error",e.what()}})<<'\n';
    }
  }
}
