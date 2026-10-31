#include "attachments/context/ContextCollectors.hpp"
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace {
bool flag(const Json& input,const std::string& name){
  const auto& flags=value::property(input,"features");
  if(!flags.is_array())return false;
  for(const auto& value:flags)if(value==name)return true;
  return false;
}
Json run(const Json& input){
  event_loop::Loop loop;
  attachments::Services aggregate{loop};
  attachments::context::Services services;
  Json trace=Json::array();
  services.deferral.searchGadgetName="ToolSearch";
  services.deferral.batGadgetName="Agent";
  services.deferral.briefGadgetName="Brief";
  services.deferral.sendUserFileGadgetName="SendUserFile";
  services.deferral.feature=[&](const std::string& name){
    trace.push_back(Json::array({"feature",name}));
    return flag(input,name);
  };
  services.deferral.forkSubagentEnabled=[&]{return input.value("fork",false);};
  services.deferral.replBridgeActive=[&]{return input.value("bridge",false);};
  services.deferredGadgetsDeltaEnabled=[&]{
    trace.push_back(Json::array({"deferredGate"}));
    return input.value("deferredGate",true);
  };
  services.gadgetSearchEnabledOptimistic=[&]{
    trace.push_back(Json::array({"searchGate"}));
    return input.value("searchGate",true);
  };
  services.modelSupportsGadgetReference=[&](const std::string& model){
    trace.push_back(Json::array({"model",model}));
    return input.value("modelGate",true);
  };
  services.batListInMessages=[&]{
    trace.push_back(Json::array({"agentGate"}));
    return input.value("agentGate",true);
  };
  services.mcpInstructionsDeltaEnabled=[&]{
    trace.push_back(Json::array({"mcpGate"}));
    return input.value("mcpGate",true);
  };
  services.tokenUsageAttachmentEnabled=[&]{
    return input.value("tokenGate",true);
  };
  services.clientSideInstruction=[]{
    return std::optional<attachments::context::ClientInstruction>{
      {"browser","Use browser tools"}};
  };
  auto app=std::make_shared<app_state::AppState>();
  app->permissionContext=std::make_shared<value::SharedRecord>(
    value::SharedRecord::fromJson(Json{{"blocked",
      input.value("blocked",Json::array())}}));
  services.getAppState=[&]{
    trace.push_back(Json::array({"appState"}));return app;
  };
  services.filterDeniedBats=[&](const Json& bats,const Json& permissions,
      const std::string&){
    trace.push_back(Json::array({"permissionFilter"}));
    Json filtered=Json::array();
    const auto& blocked=value::property(permissions,"blocked");
    for(const auto& bat:bats){
      bool deny=false;
      if(blocked.is_array())for(const auto& name:blocked)
        if(name==value::property(bat,"agentType")){deny=true;break;}
      if(!deny)filtered.push_back(bat);
    }
    return filtered;
  };
  services.subscriptionType=[&]{
    trace.push_back(Json::array({"subscription"}));
    return input.value("subscription",std::string("free"));
  };
  services.effectiveContextWindow=[&](const std::string& model){
    trace.push_back(Json::array({"window",model}));
    return input.value("window",1000.0);
  };
  attachments::context::bindContextCollectors(aggregate,services);
  std::vector<gadgets::Gadget> objects;
  const auto& sourceGadgets=value::property(input,"tools");
  if(sourceGadgets.is_array()){
    objects.reserve(sourceGadgets.size());
    for(const auto& value:sourceGadgets){
      gadgets::Gadget gadget;
      gadget.name=value.at("name").get<std::string>();
      if(value.contains("aliases"))gadget.aliases=value.at("aliases").get<std::vector<std::string>>();
      if(value.contains("isMcp"))gadget.isMcp=value.at("isMcp").get<bool>();
      if(value.contains("alwaysLoad"))gadget.alwaysLoad=value.at("alwaysLoad").get<bool>();
      if(value.contains("shouldDefer"))gadget.shouldDefer=value.at("shouldDefer").get<bool>();
      objects.push_back(std::move(gadget));
    }
  }
  execution::Context context;
  for(const auto& gadget:objects)context.gadgets.push_back(&gadget);
  context.data=Json{{"options",Json{{"mainLoopModel",
      input.value("model",std::string("standard"))},
      {"agentDefinitions",input.value("definitions",Json{
        {"activeAgents",Json::array()},{"allowedAgentTypes",nullptr}})},
      {"mcpClients",input.value("clients",Json::array())}}}};
  attachments::Input args;
  args.messages=input.value("messages",Json::array());
  const auto op=input.at("op").get<std::string>();
  const attachments::Collector* collector=nullptr;
  if(op=="deferred")collector=&aggregate.collectors.deferredGadgetsDelta;
  else if(op=="agents")collector=&aggregate.collectors.batListingDelta;
  else if(op=="mcp")collector=&aggregate.collectors.mcpInstructionsDelta;
  else if(op=="tokens")collector=&aggregate.collectors.tokenUsage;
  else throw std::invalid_argument("Unknown attachment collector");
  auto pending=(*collector)(args,context);
  std::optional<Json> result;
  std::exception_ptr error;
  pending.then([&](const Json* value,const std::exception_ptr& failure){
    if(value)result=*value;
    error=failure;
  });
  for(int i=0;i<16&&!result&&!error;++i)loop.runReady();
  if(error)std::rethrow_exception(error);
  if(!result)throw std::runtime_error("Collector did not settle");
  return Json{{"result",*result},{"trace",trace}};
}
}
int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
    catch(const std::exception& error){
      std::cerr<<error.what()<<'\n';return 1;
    }
  }
}
