#include "accounting/Accounting.hpp"
#include <stdexcept>

namespace batbox::accounting {
namespace {
template<class F>void need(const F& callback,const char* name){
  if(!callback)throw std::logic_error(std::string(name)+" dependency not supplied");
}
double value(const Json& object,const char* key){
  if(!object.is_object()||!object.contains(key)||object.at(key).is_null())return 0;
  if(!object.at(key).is_number())throw std::invalid_argument(std::string(key)+" must be a number");
  return object.at(key).get<double>();
}
Json zeroUsage(){return {{"inputTokens",0},{"outputTokens",0},{"cacheReadInputTokens",0},
  {"cacheCreationInputTokens",0},{"webSearchRequests",0},{"costUSD",0},
  {"contextWindow",0},{"maxOutputTokens",0}};}
}
Json getAdvisorUsage(const Json& usage){
  if(!usage.is_object()||!usage.contains("iterations")||usage.at("iterations").is_null())return Json::array();
  Json result=Json::array();
  for(const auto& iteration:usage.at("iterations"))
    if(iteration.is_object()&&iteration.value("type",std::string())=="advisor_message")result.push_back(iteration);
  return result;
}
Json addToTotalModelUsage(double cost,const Json& usage,const std::string& model,
  AccountingState& state,const CostServices& services){
  if(!state.modelUsage.is_object())state.modelUsage=Json::object();
  Json newUsage=zeroUsage();
  const bool existing=state.modelUsage.contains(model);
  Json& modelUsage=existing?state.modelUsage[model]:newUsage;
  modelUsage["inputTokens"]=value(modelUsage,"inputTokens")+value(usage,"input_tokens");
  modelUsage["outputTokens"]=value(modelUsage,"outputTokens")+value(usage,"output_tokens");
  modelUsage["cacheReadInputTokens"]=value(modelUsage,"cacheReadInputTokens")+value(usage,"cache_read_input_tokens");
  modelUsage["cacheCreationInputTokens"]=value(modelUsage,"cacheCreationInputTokens")+value(usage,"cache_creation_input_tokens");
  const Json server=usage.is_object()&&usage.contains("server_tool_use")?usage.at("server_tool_use"):Json();
  modelUsage["webSearchRequests"]=value(modelUsage,"webSearchRequests")+value(server,"web_search_requests");
  modelUsage["costUSD"]=value(modelUsage,"costUSD")+cost;
  need(services.getSdkBetas,"getSdkBetas");
  need(services.getContextWindowForModel,"getContextWindowForModel");
  modelUsage["contextWindow"]=services.getContextWindowForModel(model,services.getSdkBetas());
  need(services.getModelMaxOutputTokens,"getModelMaxOutputTokens");
  modelUsage["maxOutputTokens"]=services.getModelMaxOutputTokens(model).at("default");
  return modelUsage;
}
void addToTotalCostState(double cost,const Json& modelUsage,const std::string& model,
  AccountingState& state){
  state.modelUsage[model]=modelUsage;
  state.totalCostUSD+=cost;
}
double addToTotalFlightCost(double cost,const Json& usage,const std::string& model,
  AccountingState& state,const CostServices& services){
  const auto modelUsage=addToTotalModelUsage(cost,usage,model,state,services);
  addToTotalCostState(cost,modelUsage,model,state);
  double totalCost=cost;
  for(const auto& advisorUsage:getAdvisorUsage(usage)){
    if(!advisorUsage.contains("model")||!advisorUsage.at("model").is_string())
      throw std::invalid_argument("advisor usage model required");
    const auto advisorModel=advisorUsage.at("model").get<std::string>();
    const double advisorCost=calculateUSDCost(advisorModel,advisorUsage,state,services);
    totalCost+=addToTotalFlightCost(advisorCost,advisorUsage,advisorModel,state,services);
  }
  return totalCost;
}
} // namespace batbox::accounting
