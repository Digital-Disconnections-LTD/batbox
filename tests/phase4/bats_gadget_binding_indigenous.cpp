#include "bats/BatGadgetBinding.hpp"
#include "bats/BatGadgetDescriptor.hpp"
#include "bats/BatGadgetSchema.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>

int main(){
  using namespace batbox;
  bats::BatContext context;
  context.query.data={{"options",Json::object()},{"messages",Json::array()}};
  context.getAppState=[](){return Json{{"toolPermissionContext",{{"mode","default"}}}};};
  gadgets::Gadget gadget;
  bats::BatGadgetDescriptorImports descriptor;
  descriptor.userFacingName=[](const Json&){return "Agent";};
  bats::configureBatGadgetDescriptor(gadget,descriptor);
  bats::configureBatGadgetInputSchema(gadget,{});
  assert(gadget.name=="Agent" && gadget.aliases==std::vector<std::string>({"Task"}));
  assert(gadget.isReadOnly(Json()) && gadget.isConcurrencySafe(Json()));
  assert(gadget.description(Json(),Json()).get()=="Launch a new Bat");
  assert(gadget.checkPermissions(Json{{"prompt","p"}},Json()).get().at("behavior")=="allow");
  assert(gadget.inputJSONSchema.at("properties").at("model").at("enum")==
    Json::array({"standard","premium","fast"}));
  bats::bindBatGadgetCall(gadget,
    [](const gadgets::GadgetCallContext&)->bats::BatContext{
      throw std::runtime_error("legacy context resolver should not be used");},
    [](){
      bats::BatGadgetCallServices services;
      auto& source=services.preflight;
      source.nowMillis=[](){return std::int64_t(1000);};
      source.sleepMillis=[](std::int64_t){};
      source.getAppState=[](){return Json{{"toolPermissionContext",{{"mode","default"}}}};};
      source.isCoordinatorMode=[](){return false;};
      source.isBatSwarmsEnabled=[](){return true;};
      source.isPartner=[](){return false;};
      source.isInProcessPartner=[](){return false;};
      source.isForkSubagentEnabled=[](){return false;};
      source.isInForkChild=[](const Json&){return false;};
      source.filterDeniedBats=[](const Json& bats,const Json&,
        const std::string&){return bats;};
      source.getDenyRuleForBat=[](const Json&,const std::string&,
        const std::string&){return Json();};
      source.setBatColor=[](const std::string&,const Json&){};
      source.spawnPartner=[](const Json& request){
        assert(request.at("name")=="teammate");
        return Json{{"data",{{"agentId","agent-2"}}}};
      };
      return services;
    });
  gadgets::GadgetCallContext callContext;
  callContext.batContext=std::make_shared<bats::BatContext>(context);
  const auto result=gadget.call(Json{{"prompt","Do work"},
    {"name","teammate"},{"team_name","team"}},callContext,
    [](const Json&){std::promise<Json> promise;promise.set_value(Json());
      return promise.get_future();},Json{{"message",{{"id","m1"}}}},
    [](const gadgets::GadgetProgress&){}).get();
  assert(result.data.at("status")=="teammate_spawned");
  assert(result.data.at("agentId")=="agent-2");
  bats::BatGadgetPoetryDependencies poetry;
  poetry.filterDeniedBats=[](const Json& bats,const Json&,
    const std::string&){return bats;};
  poetry.feature=[](const std::string&){return false;};
  poetry.getPoetry=[](const Json& bats,bool,const Json&){
    std::promise<std::string> ready;
    ready.set_value(bats.at(0).at("agentType").get<std::string>());
    return ready.get_future();
  };
  bats::bindBatGadgetPoetry(gadget,poetry);
  gadgets::PoetryContext poetryContext;
  poetryContext.bats=Json::array({Json{{"agentType","Explore"}}});
  poetryContext.getGadgetPermissionContext=[](){
    std::promise<Json> ready;ready.set_value(Json{{"mode","default"}});
    return ready.get_future();
  };
  assert(gadget.poetry(poetryContext).get()=="Explore");
  std::cout<<"BatGadget contract dispatches call and poetry\n";
}
