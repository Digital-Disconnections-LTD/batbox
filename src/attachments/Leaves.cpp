#include "attachments/Leaves.hpp"
#include <regex>
#include <stdexcept>

namespace batbox::attachments {
namespace {
event_loop::Deferred<Json> ready(event_loop::Loop& loop,Json value){
  event_loop::Deferred<Json> result(loop);result.resolve(std::move(value));return result;
}
Json one(Json item){return Json::array({std::move(item)});}
}
bool hasUltrathinkKeyword(const std::string& text){
  static const std::regex pattern(R"(\bultrathink\b)",
    std::regex::ECMAScript|std::regex::icase);
  return std::regex_search(text,pattern);
}
void bindLeafCollectors(Services& aggregation,LeafServices leaves){
  auto& loop=aggregation.loop;
  aggregation.collectors.dateChange=[leaves,&loop](const Input& input,
      execution::Context&){
    if(!leaves.poetryState||!leaves.localIsoDate)
      throw std::invalid_argument("Date-change state and date imports required");
    const auto current=leaves.localIsoDate();
    const auto last=bootstrap::getLastEmittedDate(*leaves.poetryState);
    if(!last){
      bootstrap::setLastEmittedDate(*leaves.poetryState,current);
      return ready(loop,Json::array());
    }
    if(current==*last)return ready(loop,Json::array());
    bootstrap::setLastEmittedDate(*leaves.poetryState,current);
    if(!leaves.feature)throw std::invalid_argument("Date-change feature import required");
    if(leaves.feature("KAIROS")){
      if(!leaves.kairosActive)
        throw std::invalid_argument("Local session activity import required");
      if(leaves.kairosActive()&&!input.messages.is_null()&&
         leaves.flushOnDateChange)
        leaves.flushOnDateChange(input.messages,current);
    }
    return ready(loop,one(Json{{"type","date_change"},{"newDate",current}}));
  };
  aggregation.collectors.ultrathinkEffort=[leaves,&loop](const Input& input,
      execution::Context&){
    if(!leaves.ultrathinkEnabled)
      throw std::invalid_argument("Reasoning keyword feature import required");
    if(!leaves.ultrathinkEnabled()||!input.text||input.text->empty()||
       !hasUltrathinkKeyword(*input.text))return ready(loop,Json::array());
    return ready(loop,one(Json{{"type","ultrathink_effort"},{"level","high"}}));
  };
  aggregation.collectors.criticalSystemReminder=[&loop](const Input&,
      execution::Context& context){
    const auto& reminder=value::property(context.data,
      "criticalSystemReminder_EXPERIMENTAL");
    if(!value::truthy(reminder))return ready(loop,Json::array());
    return ready(loop,one(Json{{"type","critical_system_reminder"},
      {"content",reminder}}));
  };
  aggregation.collectors.outputStyle=[leaves,&loop](const Input&,
      execution::Context&){
    if(!leaves.getSettings)
      throw std::invalid_argument("Output-style settings import required");
    const auto settings=leaves.getSettings();
    const auto& value=value::property(settings,"outputStyle");
    const Json style=value::truthy(value)?value:Json("default");
    if(style=="default")return ready(loop,Json::array());
    return ready(loop,one(Json{{"type","output_style"},{"style",style}}));
  };
  aggregation.collectors.outputTokenUsage=[leaves,&loop](const Input&,
      execution::Context&){
    if(!leaves.feature)
      throw std::invalid_argument("Token-budget feature import required");
    if(!leaves.feature("TOKEN_BUDGET"))return ready(loop,Json::array());
    if(!leaves.costState)
      throw std::invalid_argument("Token-budget cost state required");
    const auto budget=bootstrap::getCurrentTurnTokenBudget(*leaves.costState);
    if(!budget||*budget<=0)return ready(loop,Json::array());
    return ready(loop,one(Json{{"type","output_token_usage"},
      {"turn",bootstrap::getTurnOutputTokens(*leaves.costState)},
      {"session",bootstrap::getTotalOutputTokens(*leaves.costState)},
      {"budget",*budget}}));
  };
  aggregation.collectors.budgetUsd=[leaves,&loop](const Input&,
      execution::Context& context){
    const auto& options=value::property(context.data,"options");
    if(!options.is_object()||!options.contains("maxBudgetUsd"))
      return ready(loop,Json::array());
    if(!leaves.costState)throw std::invalid_argument("Cost state required");
    const double total=options.at("maxBudgetUsd").get<double>();
    const double used=bootstrap::getTotalCostUSD(*leaves.costState);
    return ready(loop,one(Json{{"type","budget_usd"},
      {"used",used},{"total",total},{"remaining",total-used}}));
  };
}
} // namespace batbox::attachments
