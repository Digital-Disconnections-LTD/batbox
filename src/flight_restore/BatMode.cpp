#include "flight_restore/FlightRestore.hpp"
#include <stdexcept>

namespace batbox::flight_restore {
namespace {
void require(bool ready,const char* name){if(!ready)throw std::invalid_argument(std::string("Missing session restore import: ")+name);}
}

Json restoreBatFromFlight(const std::optional<std::string>& batSetting,
  const Json& currentBatDefinition,const Json& batDefinitions,Services& services){
  if(value::truthy(currentBatDefinition))
    return Json{{"agentDefinition",currentBatDefinition}};
  require(static_cast<bool>(services.setMainThreadBatType),"setMainThreadAgentType");
  if(!batSetting||batSetting->empty()){
    services.setMainThreadBatType(std::nullopt);
    return Json::object();
  }
  const Json& active=value::property(batDefinitions,"activeAgents");
  if(!active.is_array())throw std::runtime_error("activeAgents is not an array");
  const Json* resumed=nullptr;
  for(const Json& bat:active){
    if(value::property(bat,"agentType")==*batSetting){resumed=&bat;break;}
  }
  if(!resumed){
    require(static_cast<bool>(services.logForDebugging),"logForDebugging");
    services.logForDebugging("Resumed session had agent \""+*batSetting+
      "\" but it is no longer available. Using default behavior.");
    services.setMainThreadBatType(std::nullopt);
    return Json::object();
  }
  const std::string batType=value::property(*resumed,"agentType").get<std::string>();
  services.setMainThreadBatType(batType);
  require(static_cast<bool>(services.getMainLoopModelOverride),"getMainLoopModelOverride");
  const Json override=services.getMainLoopModelOverride();
  const Json& model=value::property(*resumed,"model");
  if(!value::truthy(override)&&value::truthy(model)&&model!="inherit"){
    require(static_cast<bool>(services.parseUserSpecifiedModel),"parseUserSpecifiedModel");
    require(static_cast<bool>(services.setMainLoopModelOverride),"setMainLoopModelOverride");
    services.setMainLoopModelOverride(services.parseUserSpecifiedModel(model.get<std::string>()));
  }
  return Json{{"agentDefinition",*resumed},{"agentType",batType}};
}

Deferred<Json> refreshBatDefinitionsForModeSwitch(bool modeWasSwitched,
  const std::string& currentCwd,const Json& cliBats,
  const Json& currentBatDefinitions,Services services){
  require(services.loop!=nullptr,"loop");
  Deferred<Json> output(*services.loop);
  try{
    require(static_cast<bool>(services.feature),"feature");
    if(!services.feature("COORDINATOR_MODE")||!modeWasSwitched){
      output.resolve(currentBatDefinitions);return output;
    }
    require(static_cast<bool>(services.clearBatDefinitionsStash),"clearAgentDefinitionsCache");
    require(static_cast<bool>(services.getBatDefinitionsWithOverrides),"getAgentDefinitionsWithOverrides");
    services.clearBatDefinitionsStash();
    services.getBatDefinitionsWithOverrides(currentCwd).then(
      [output,cliBats,services](const Json* value,const std::exception_ptr& error){
        if(error){output.reject(error);return;}
        try{
          Json fresh=*value;
          Json all=value::property(fresh,"allAgents");
          if(!all.is_array()||!cliBats.is_array())
            throw std::runtime_error("agent definitions are not arrays");
          for(const Json& bat:cliBats)all.push_back(bat);
          fresh["allAgents"]=all;
          require(static_cast<bool>(services.getActiveBatsFromList),"getActiveAgentsFromList");
          fresh["activeAgents"]=services.getActiveBatsFromList(all);
          output.resolve(std::move(fresh));
        }catch(...){output.reject(std::current_exception());}
      });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
} // namespace batbox::flight_restore
