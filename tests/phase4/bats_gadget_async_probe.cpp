#include "bats/BatGadgetAsync.hpp"
#include <iostream>
#include <memory>
#include <string>
using batbox::Json;
int main(){
 std::string line;
 while(std::getline(std::cin,line)){
  try{
   const auto item=batbox::value::parse(line);
   Json trace=Json::array();
   Json state={{"tasks",Json::object()},
     {"agentNameRegistry",item.value("registry",Json::array())}};
   batbox::bats::RootSetAppState rootSet=[&](const std::function<Json(const Json&)>& update){
     const Json previous=state.at("agentNameRegistry");
     state=update(state);
     if(state.at("agentNameRegistry")!=previous)
       trace.push_back(Json::array({"registry",state.at("agentNameRegistry")}));
   };
   batbox::bats::BatMissionStateServices missionState;
   missionState.setAppState=rootSet;
   missionState.getAppState=[&](){return state;};
   missionState.nowMillis=[](){return std::int64_t(1000);};
   batbox::bats::BatRegistrationServices registration;
   registration.state=&missionState;
   registration.getMissionOutputPath=[](const std::string& id){return "/output/"+id;};
   registration.getBatTranscriptPath=[](const std::string& id){return "/transcript/"+id;};
   registration.initMissionOutputAsSymlink=[&](const std::string&,const std::string&){
     trace.push_back(Json::array({"register",item.at("id"),item.at("description"),
       item.at("prompt"),item.at("agent").at("agentType"),
       item.value("toolUseId",Json())}));
     if(item.value("regFail",false))throw std::runtime_error("registration failed");
   };
   registration.registerCleanup=[](const std::function<void()>&){return [](){};};
   registration.enqueueSdkEvent=[](const Json&){};
   std::vector<std::unique_ptr<batbox::gadgets::Gadget>> owned;
   batbox::bats::BatContext context;
   context.query.data=Json::object();
   if(item.contains("toolUseId"))context.query.data["toolUseId"]=item.at("toolUseId");
   for(const auto& spec:item.at("tools")){
     auto gadget=std::make_unique<batbox::gadgets::Gadget>();
     gadget->name=spec.at("name").get<std::string>();
     context.query.gadgets.push_back(gadget.get());owned.push_back(std::move(gadget));
   }
   batbox::bats::BatGadgetLaunchPlan plan;
   plan.shouldRunAsync=true;
   plan.earlyBatId=item.at("id").get<std::string>();
   plan.isCoordinator=item.value("isCoordinator",false);
   plan.rootSetAppState=rootSet;
   plan.runBatParams.batDefinition=item.at("agent");
   plan.runBatParams.override=item.value("override",Json::object());
   plan.metadata=item.value("metadata",Json::object());
   if(item.contains("cwd") && item.at("cwd").is_string())
     plan.cwdOverridePath=item.at("cwd").get<std::string>();
   batbox::bats::BatGadgetAsyncServices services;
   services.registration=&registration;
   services.getParentFlightId=[&]()->std::optional<std::string>{
     if(item.contains("parentSessionId") && item.at("parentSessionId").is_string())
       return item.at("parentSessionId").get<std::string>();
     return std::nullopt;
   };
   services.isBuiltInBat=[](const Json& bat){
     return bat.contains("builtIn") && bat.at("builtIn")==true;
   };
   services.isForkSubagentEnabled=[&](){return item.value("forceFork",false);};
   services.getSdkBatProgressSummariesEnabled=[&](){return item.value("sdkSummaries",false);};
   services.runWithBatContext=[&](const Json& batContext,const std::function<void()>& fn){
     trace.push_back(Json::array({"context",batContext}));fn();
   };
   services.runWithCwdOverride=[&](const std::string& cwd,const std::function<void()>& fn){
     trace.push_back(Json::array({"cwd",cwd}));fn();
   };
   services.launchLifecycle=[&](batbox::bats::AsyncLifecycleParams params){
     trace.push_back(Json::array({"lifecycle",params.missionId,params.description,
       params.batIdForCleanup,params.enableSummarization,
       static_cast<bool>(params.abortOwner)}));
     params.makeStream([](const std::shared_ptr<batbox::bats::StashSafeParams>&){},
       batbox::query::Yield{});
   };
   services.runBat=[&](const batbox::bats::RunBatParams& params,const batbox::query::Yield&){
     trace.push_back(Json::array({"runAgent",params.override.at("agentId"),
       static_cast<bool>(params.overrideAbortOwner),
       static_cast<bool>(params.onStashSafeParams)}));
   };
   services.getMissionOutputPath=[](const std::string& id){return "/output/"+id;};
   const Json input={{"name",item.value("name",Json())},
     {"description",item.at("description")},{"prompt",item.at("prompt")}};
   try{
     const auto result=batbox::bats::launchBatGadgetAsync(input,
       item.value("assistantMessage",Json::object()),context,std::move(plan),services);
     std::cout<<batbox::value::stringify(Json{{"result",result},{"trace",trace}})<<'\n';
   }catch(const std::exception& error){
     std::cout<<batbox::value::stringify(Json{{"error",error.what()},{"trace",trace}})<<'\n';
   }
  }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
 }
}
