#include "bootstrap/FlightTransitions.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>

int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    bootstrap::FlightTransitions state;
    std::shared_ptr<Json> schema;
    auto snapshot=[&](){
      const auto current=bootstrap::getInitJsonSchema(state);
      return Json{{"trust",bootstrap::getFlightTrustAccepted(state)},
        {"persistence",bootstrap::isFlightPersistenceDisabled(state)},
        {"exitedPlan",bootstrap::hasExitedPlanModeInFlight(state)},
        {"planExit",bootstrap::needsPlanModeExitAttachment(state)},
        {"autoExit",bootstrap::needsAutoModeExitAttachment(state)},
        {"lsp",bootstrap::hasShownLspRecommendationThisFlight(state)},
        {"schema",current?*current:Json(nullptr)},
        {"sameSchema",schema?Json(current==schema):Json(nullptr)}};
    };
    Json trace=Json::array();trace.push_back(snapshot());
    if(test.contains("ops"))for(const auto& op:test["ops"]){
      const auto kind=op["kind"].get<std::string>();
      if(kind=="trust")bootstrap::setFlightTrustAccepted(state,
        op["value"].get<bool>());
      else if(kind=="persistence")
        bootstrap::setFlightPersistenceDisabled(state,
          op["value"].get<bool>());
      else if(kind=="exitedPlan")bootstrap::setHasExitedPlanMode(state,
        op["value"].get<bool>());
      else if(kind=="planExit")
        bootstrap::setNeedsPlanModeExitAttachment(state,
          op["value"].get<bool>());
      else if(kind=="planTransition")
        bootstrap::handlePlanModeTransition(state,
          op["from"].get<std::string>(),op["to"].get<std::string>());
      else if(kind=="autoExit")
        bootstrap::setNeedsAutoModeExitAttachment(state,
          op["value"].get<bool>());
      else if(kind=="autoTransition")
        bootstrap::handleAutoModeTransition(state,
          op["from"].get<std::string>(),op["to"].get<std::string>());
      else if(kind=="lsp")
        bootstrap::setLspRecommendationShownThisFlight(state,
          op["value"].get<bool>());
      else if(kind=="schema"){
        schema=std::make_shared<Json>(op["value"]);
        bootstrap::setInitJsonSchema(state,schema);
      }else if(kind=="mutateSchema")
        (*schema)[op["key"].get<std::string>()]=op["value"];
      else throw std::runtime_error("Unknown transition operation");
      trace.push_back(snapshot());
    }
    std::cout<<value::stringify(trace)<<'\n';
  }
}
