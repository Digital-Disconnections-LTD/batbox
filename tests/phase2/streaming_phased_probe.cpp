#include "streaming/Reducer.hpp"
#include <iostream>

using namespace batbox;
int main(int argc,char** argv){
  const std::string scenario=argc>1?argv[1]:"close_after_refusal";
  Json trace=Json::array();
  std::string phaseMarker="before";
  streaming::ReducerConfig config;
  config.model="batbox-model";config.resolvedModel="batbox-model";
  config.maxOutputTokens=7;
  config.randomUUID=[](){return std::string("uuid");};
  config.timestamp=[](){return std::string("now");};
  config.calculateUSDCost=[&](const std::string&,const Json&){
    trace.push_back("cost");return 1.0;};
  config.addToTotalFlightCost=[&](double,const Json&,const std::string&){
    trace.push_back("add_cost");return 1.0;};
  config.getErrorMessageIfRefusal=[&](const Json&,const std::string&)->std::optional<Json>{
    trace.push_back("refusal_callback");
    return Json{{"type","assistant"},{"kind","refusal"}};};
  config.createAssistantAPIErrorMessage=[&](const std::string&,
      const std::string&,const std::string&){
    trace.push_back("error_callback");
    if(scenario=="interleave")trace.push_back("error_state:"+phaseMarker);
    return Json{{"type","assistant"},{"kind","api_error"}};};
  streaming::Reducer reducer(config);
  Json part={{"type","message_delta"},{"usage",{{"input_tokens",1}}},
    {"delta",{{"stop_reason","max_tokens"}}}};
  auto emission=reducer.beginEvent(part);
  if(!emission)throw std::logic_error("Refusal emission missing");
  trace.push_back("yield:"+(*emission)->at("kind").get<std::string>());
  if(scenario=="interleave"){
    phaseMarker="after";trace.push_back("mutate:"+phaseMarker);
  }
  if(scenario=="close_after_refusal")trace.push_back("return:closed");
  else while((emission=reducer.nextEmission())){
    const Json& value=**emission;
    trace.push_back("yield:"+(value.contains("kind")?
      value.at("kind").get<std::string>():value.at("type").get<std::string>()));
  }
  std::cout<<Json{{"trace",trace}}.dump()<<'\n';
}
