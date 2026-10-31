#include "bootstrap/ModelState.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>

int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    bootstrap::ModelState state;
    std::shared_ptr<Json> strings;
    std::shared_ptr<std::vector<std::string>> betas;
    if(test.contains("ops"))for(const auto& op:test["ops"]){
      const auto kind=op.value("kind",std::string());
      if(kind=="override")bootstrap::setMainLoopModelOverride(state,
        op.contains("value")?std::optional(op["value"]):std::nullopt);
      else if(kind=="initial")bootstrap::setInitialMainLoopModel(state,
        op["value"]);
      else if(kind=="strings"){
        strings=std::make_shared<Json>(op["value"]);
        bootstrap::setModelStrings(state,strings);
      }else if(kind=="mutateStrings")
        (*strings)[op["key"].get<std::string>()]=op["value"];
      else if(kind=="resetStrings")
        bootstrap::resetModelStringsForTestingOnly(state);
      else if(kind=="betas"){
        betas=std::make_shared<std::vector<std::string>>();
        for(const auto& beta:op["value"])
          betas->push_back(beta.get<std::string>());
        bootstrap::setSdkBetas(state,betas);
      }else if(kind=="appendBeta")
        betas->push_back(op["value"].get<std::string>());
      else if(kind=="clearBetas")bootstrap::setSdkBetas(state,nullptr);
      else throw std::runtime_error("Unknown model state operation");
    }
    Json betaValues=nullptr;
    if(auto active=bootstrap::getSdkBetas(state)){
      betaValues=Json::array();
      for(const auto& beta:*active)betaValues.push_back(beta);
    }
    const auto activeStrings=bootstrap::getModelStrings(state);
    const Json result={{"override",state.mainLoopModelOverride?
        *state.mainLoopModelOverride:Json(nullptr)},
      {"hasOverride",state.mainLoopModelOverride.has_value()},
      {"initial",bootstrap::getInitialMainLoopModel(state)},
      {"strings",activeStrings?*activeStrings:Json(nullptr)},
      {"sameStrings",strings?Json(activeStrings==strings):Json(nullptr)},
      {"betas",betaValues},
      {"sameBetas",betas?Json(bootstrap::getSdkBetas(state)==betas):
        Json(nullptr)}};
    std::cout<<value::stringify(result)<<'\n';
  }
}
