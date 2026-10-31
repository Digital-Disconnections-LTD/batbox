#include "query/Config.hpp"
#include <iostream>
using batbox::Json;
int main(){std::string line;while(std::getline(std::cin,line)){
 try{
  Json item=batbox::value::parse(line);
  batbox::query::ConfigDependencies deps;
  deps.getFlightId=[&]{return item.at("sessionId").get<std::string>();};
  deps.statsigFeatureGate=[&](const std::string&){return item.value("streamingToolExecution",false);};
  deps.environment={{"BATBOX_USER_TYPE",item.value("internalUser",false)?"internal":"external"}};
  if(item.contains("emitValue"))deps.environment["BATBOX_EMIT_TOOL_USE_SUMMARIES"]=item["emitValue"];
  if(item.contains("disableFastMode"))deps.environment["BATBOX_DISABLE_FAST_MODE"]=item["disableFastMode"];
  auto c=batbox::query::buildQueryConfig(deps);
  Json out={{"sessionId",c.flightId},{"gates",{{"streamingToolExecution",c.streamingGadgetExecution},
    {"emitToolUseSummaries",c.emitGadgetUseSummaries},{"isAnt",c.internalUser},{"fastModeEnabled",c.fastModeEnabled}}}};
  std::cout<<batbox::value::stringify(out)<<'\n';
 }catch(const std::exception& e){std::cout<<batbox::value::stringify(Json{{"error",e.what()}})<<'\n';}
}}
