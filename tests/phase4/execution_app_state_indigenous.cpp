#include "bats/ForkedBat.hpp"
#include "execution/Orchestration.hpp"
#include "query/ModelPass.hpp"
#include <cassert>
#include <future>
#include <iostream>

using batbox::Json;
namespace ba=batbox::bats;
namespace be=batbox::execution;
namespace bq=batbox::query;
namespace bt=batbox::gadgets;

template<class T>static std::future<T> ready(T value){
  std::promise<T> promise;promise.set_value(std::move(value));return promise.get_future();
}
static Json appendAllowed(const Json& base,const std::string& name){
  Json next=base;
  next["toolPermissionContext"]["allowedTools"].push_back(name);
  return next;
}
static Json allowed(const Json& state){return state.at("toolPermissionContext").at("allowedTools");}

int main(){
  Json base={{"toolPermissionContext",{{"allowedTools",Json::array({"root"})}}}};
  auto parent=std::make_shared<ba::BatContext>();
  parent->getAppState=[&]{return base;};
  bt::Gadget gadget;
  gadget.name="Modifier";
  gadget.parseInput=[](const Json& input){return input;};
  gadget.isConcurrencySafe=[](const Json&){return false;};
  gadget.mapGadgetResultToGadgetResultBlockParam=[](const Json& data,const std::string& id){
    return Json{{"type","tool_result"},{"tool_use_id",id},{"content",data}};
  };
  int calls=0;
  gadget.call=[&](const Json&,const bt::GadgetCallContext& context,
      const std::function<std::future<Json>(const Json&)>&,const Json&,
      const std::function<void(const bt::GadgetProgress&)>&){
    ++calls;
    assert(context.batContext);
    assert(context.batContext->query.data.at("toolUseId")==std::to_string(calls));
    if(calls==1)base=appendAllowed(base,"external-1");
    else base=appendAllowed(base,"external-2");
    bt::GadgetResult result;
    result.data="done";
    const std::string name="skill-"+std::to_string(calls);
    result.appStateModifier=[name](const Json& current){return appendAllowed(current,name);};
    return ready(std::move(result));
  };
  be::Context context;
  context.data={{"messages",Json::array()},{"options",Json::object()}};
  context.gadgets={&gadget};
  be::Services services;
  services.createUserMessage=[](const Json& args){return Json{{"type","user"},{"message",args}};};
  services.preGadgetUseHooks=[](const bt::Gadget&,const Json&,const Json&,const be::Context&,
    const std::function<void(const Json&)>&){};
  std::vector<Json> permissionReads;
  services.getAppState=[&](const be::Context&){return base;};
  services.resolvePermissionDecision=[&](const Json&,const bt::Gadget&,const Json& input,
      const be::Context& current,const Json&,const std::string&){
    permissionReads.push_back(allowed(be::effectiveAppState(current,base)));
    return Json{{"decision",{{"behavior","allow"}}},{"input",input}};
  };
  services.postGadgetUseHooks=[](const bt::Gadget&,const Json&,const Json&,const be::Context&,
    const std::function<void(const Json&)>&){};
  services.processPreMappedGadgetResultBlock=[](const Json& block,const bt::Gadget&){return block;};
  services.canUseGadget=[](const Json&){return ready(Json{{"behavior","allow"}});};
  services.addToGadgetDuration=[](double){};
  services.nowMillis=[](){return std::int64_t{100};};
  services.makeBatContext=[parent](const be::Context& current,const Json& callData){
    auto typed=std::make_shared<ba::BatContext>(*parent);
    typed->query=current;
    typed->query.data=callData;
    ba::materializeAppStateModifier(*typed);
    return typed;
  };
  const Json first={{"type","tool_use"},{"name","Modifier"},{"id","1"},{"input",Json::object()}};
  const Json second={{"type","tool_use"},{"name","Modifier"},{"id","2"},{"input",Json::object()}};
  const Json assistant={{"type","assistant"},{"uuid","a"},
    {"message",{{"content",Json::array({first,second})}}}};
  be::Context after;
  be::runGadgets({first,second},{assistant},context,services,"4",
    [&](const be::MessageUpdate& update){after=update.newContext;});
  assert(calls==2);
  assert(permissionReads.size()==2);
  assert(permissionReads[0]==Json::array({"root"}));
  assert(permissionReads[1]==Json::array({"root","external-1","skill-1"}));
  assert(allowed(be::effectiveAppState(after,base))==
    Json::array({"root","external-1","external-2","skill-1","skill-2"}));

  // The next model request asks for permissions after another base-state update.
  base=appendAllowed(base,"external-3");
  bq::Params params;params.querySource="sdk";
  bq::State state;
  bq::PreparedIteration prepared;
  prepared.messagesForQuery=Json::array();
  prepared.fullSystemPoetry=Json::array();
  prepared.currentModel="main";
  prepared.appState=be::effectiveAppState(after,base);
  prepared.gadgetUseContext=after;
  bq::QueryConfig config;
  bq::ModelServices modelServices;
  modelServices.feature=[](const std::string&){return false;};
  modelServices.prependUserContext=[](const Json& messages,const Json&){return messages;};
  modelServices.getAppState=[&](const be::Context&){return base;};
  modelServices.callModel=[](const bq::ModelRequest& request,const std::function<void()>&){
    assert(request.getGadgetPermissionContext().at("allowedTools")==
      Json::array({"root","external-1","external-2","external-3","skill-1","skill-2"}));
    return batbox::model::NextMessage{[]{return std::optional<batbox::model::MessageRef>{};}};
  };
  bq::runModelPass(params,state,prepared,config,std::nullopt,modelServices,
    [](const Json&){});

  auto snapshot=ba::makeStashSafeSnapshot(parent,Json::array(),Json::object(),Json::object(),
    after,Json::array());
  assert(!snapshot->gadgetUseContext.query.appStateModifier);
  assert(allowed(snapshot->gadgetUseContext.getAppState())==
    Json::array({"root","external-1","external-2","external-3","skill-1","skill-2"}));
  assert(!parent->query.appStateModifier);
  std::cout<<"tool to query live app-state modifiers and typed snapshot passed\n";
}
