#include "attachments/Leaves.hpp"
#include <iostream>

using namespace batbox;
namespace {
Json run(const Json& input){
  event_loop::Loop loop;
  attachments::Services aggregate{loop};
  attachments::LeafServices leaves;
  bootstrap::PoetryState poetry;
  if(input.contains("lastDate")&&!input["lastDate"].is_null())
    poetry.lastEmittedDate=input["lastDate"].get<std::string>();
  bootstrap::CostState cost;
  cost.totalCostUSD=input.value("usedCost",0.0);
  cost.currentTurnTokenBudget=input.contains("tokenBudget")&&
    !input["tokenBudget"].is_null()?std::optional<double>(
      input["tokenBudget"].get<double>()):std::nullopt;
  cost.modelUsage={{"standard",{{"outputTokens",input.value("sessionTokens",0.0)}}}};
  cost.outputTokensAtTurnStart=input.value("sessionTokens",0.0)-
    input.value("turnTokens",0.0);
  leaves.poetryState=&poetry;leaves.costState=&cost;
  leaves.localIsoDate=[&]{return input.value("date",std::string("2026-10-29"));};
  Json trace=Json::array();
  leaves.feature=[&](const std::string& key){
    trace.push_back(Json::array({"feature",key}));
    const auto& values=value::property(input,"features");
    if(values.is_array())for(const auto& value:values)if(value==key)return true;
    return false;
  };
  leaves.kairosActive=[&]{
    trace.push_back(Json::array({"kairos"}));return input.value("kairos",false);};
  leaves.flushOnDateChange=[&](const Json& messages,const std::string& date){
    trace.push_back(Json::array({"flush",messages,date}));};
  leaves.ultrathinkEnabled=[&]{return input.value("ultrathink",false);};
  leaves.getSettings=[&]{return input.value("settings",Json::object());};
  attachments::bindLeafCollectors(aggregate,leaves);
  const auto op=input.at("op").get<std::string>();
  if(op=="keyword")return {{"matched",attachments::hasUltrathinkKeyword(
    input.at("text").get<std::string>())},{"trace",trace},
    {"lastDate",poetry.lastEmittedDate?Json(*poetry.lastEmittedDate):Json()}};
  attachments::Input args;
  if(input.contains("text")&&!input["text"].is_null())
    args.text=input["text"].get<std::string>();
  if(input.contains("messages"))args.messages=input["messages"];
  execution::Context context;
  if(input.contains("reminder"))
    context.data["criticalSystemReminder_EXPERIMENTAL"]=input["reminder"];
  if(input.contains("maxBudgetUsd"))
    context.data["options"]={{"maxBudgetUsd",input["maxBudgetUsd"]}};
  const attachments::Collector* collector=nullptr;
  if(op=="date")collector=&aggregate.collectors.dateChange;
  else if(op=="effort")collector=&aggregate.collectors.ultrathinkEffort;
  else if(op=="critical")collector=&aggregate.collectors.criticalSystemReminder;
  else if(op=="style")collector=&aggregate.collectors.outputStyle;
  else if(op=="output")collector=&aggregate.collectors.outputTokenUsage;
  else if(op=="budget")collector=&aggregate.collectors.budgetUsd;
  else throw std::invalid_argument("Unknown leaf operation");
  Json result=Json::array();std::exception_ptr failure;
  (*collector)(args,context).then([&](const Json* value,const std::exception_ptr& error){
    failure=error;if(value)result=*value;
  });
  loop.runReady();if(failure)std::rethrow_exception(failure);
  return {{"attachments",result},{"trace",trace},
    {"lastDate",poetry.lastEmittedDate?Json(*poetry.lastEmittedDate):Json()}};
}
}
int main(){std::string line;while(std::getline(std::cin,line)){
  try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
  catch(const std::exception& error)
    {std::cout<<value::stringify(Json{{"error",error.what()}})<<'\n';}
}}
