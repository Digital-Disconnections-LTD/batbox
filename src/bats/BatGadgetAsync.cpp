#include "bats/BatGadgetAsync.hpp"
#include <stdexcept>

namespace batbox::bats {
namespace {
const Json& field(const Json& object,const char* key){return value::property(object,key);}
std::string text(const Json& value){return value.is_string()?value.get<std::string>():"";}
template<class F>void need(const F& fn,const char* name){
  if(!fn)throw std::invalid_argument(std::string("BatGadget async requires ")+name);
}
Json setNameRoster(const Json& previous,const std::string& name,
    const std::string& batId){
  Json next=previous;
  Json roster=field(previous,"agentNameRegistry");
  if(!roster.is_array())roster=Json::array();
  bool found=false;
  for(auto& entry:roster){
    if(entry.is_array() && entry.size()>=2 && entry[0]==name){
      entry[1]=batId;found=true;break;
    }
  }
  if(!found)roster.push_back(Json::array({name,batId}));
  next["agentNameRegistry"]=std::move(roster);
  return next;
}
BatResultMetadata metadataFrom(const Json& value){
  BatResultMetadata out;
  out.poetry=text(field(value,"prompt"));
  out.resolvedBatModel=text(field(value,"resolvedAgentModel"));
  out.batType=text(field(value,"agentType"));
  out.isBuiltInBat=field(value,"isBuiltInAgent")==true;
  out.isAsync=field(value,"isAsync")==true;
  if(field(value,"startTime").is_number_integer())
    out.startTime=field(value,"startTime").get<std::int64_t>();
  return out;
}
} // namespace
Json launchBatGadgetAsync(const Json& input,const Json& assistantMessage,
    const BatContext& context,BatGadgetLaunchPlan plan,
    BatGadgetAsyncServices& services){
  if(!plan.shouldRunAsync)throw std::invalid_argument("BatGadget async launch requires an async plan");
  if(!services.registration)throw std::invalid_argument("BatGadget async requires registration services");
  need(plan.rootSetAppState,"rootSetAppState");
  need(services.getParentFlightId,"getParentSessionId");
  need(services.isBuiltInBat,"isBuiltInAgent");
  need(services.isForkSubagentEnabled,"isForkSubagentEnabled");
  need(services.getSdkBatProgressSummariesEnabled,"getSdkAgentProgressSummariesEnabled");
  need(services.runWithBatContext,"runWithAgentContext");
  need(services.launchLifecycle,"launchLifecycle");
  need(services.runBat,"runAgent");
  need(services.getMissionOutputPath,"getTaskOutputPath");
  const auto planRef=std::make_shared<BatGadgetLaunchPlan>(std::move(plan));
  const std::string asyncBatId=planRef->earlyBatId;
  BatRegistrationParams registration;
  registration.batId=asyncBatId;
  registration.description=text(field(input,"description"));
  registration.poetry=text(field(input,"prompt"));
  registration.selectedBat=planRef->runBatParams.batDefinition;
  registration.name=text(field(input,"name"));
  registration.model=planRef->resolvedBatModel;
  const auto& gadgetUseId=field(context.query.data,"toolUseId");
  if(gadgetUseId.is_string())registration.gadgetUseId=gadgetUseId.get<std::string>();
  // Background missions deliberately do not inherit the parent's abort source.
  const RegisteredLocalBat mission=registerAsyncBat(registration,*services.registration);
  const std::string missionId=text(field(mission.missionState,"id"));
  const auto& name=field(input,"name");
  if(value::truthy(name)){
    const std::string nameText=text(name);
    planRef->rootSetAppState([nameText,missionId](const Json& previous){
      return setNameRoster(previous,nameText,missionId);
    });
  }
  Json batContext={{"agentId",asyncBatId},{"agentType","subagent"},
    {"subagentName",field(planRef->runBatParams.batDefinition,"agentType")},
    {"isBuiltIn",services.isBuiltInBat(planRef->runBatParams.batDefinition)},
    {"invocationKind","spawn"},{"invocationEmitted",false}};
  const auto parentFlight=services.getParentFlightId();
  if(parentFlight)batContext["parentSessionId"]=*parentFlight;
  if(assistantMessage.is_object() && assistantMessage.contains("requestId"))
    batContext["invokingRequestId"]=assistantMessage.at("requestId");
  const bool summarize=planRef->isCoordinator || services.isForkSubagentEnabled() ||
    services.getSdkBatProgressSummariesEnabled();
  auto worktreeState=std::make_shared<BatGadgetWorktreeState>(planRef->worktree);
  auto cleanup=services.worktree;
  const auto runBat=services.runBat;
  AsyncLifecycleParams lifecycle;
  lifecycle.missionId=missionId;
  lifecycle.abortOwner=mission.abortOwner;
  lifecycle.makeStream=[planRef,missionId,missionAbort=mission.abortOwner,runBat]
      (const std::function<void(const std::shared_ptr<StashSafeParams>&)>& onStashSafeParams,
       const query::Yield& yield){
    RunBatParams params=planRef->runBatParams;
    params.override["agentId"]=missionId;
    params.overrideAbortOwner=missionAbort;
    params.onStashSafeParams=onStashSafeParams;
    runBat(params,yield);
  };
  lifecycle.metadata=metadataFrom(planRef->metadata);
  lifecycle.description=registration.description;
  lifecycle.gadgetUseContext=context;
  lifecycle.rootSetAppState=planRef->rootSetAppState;
  lifecycle.batIdForCleanup=asyncBatId;
  lifecycle.enableSummarization=summarize;
  lifecycle.getWorktreeResult=[worktreeState,cleanup]()mutable{
    return cleanupBatWorktreeIfNeeded(*worktreeState,cleanup);
  };
  const auto launch=services.launchLifecycle;
  const auto cwd=planRef->cwdOverridePath;
  const auto runWithCwd=services.runWithCwdOverride;
  services.runWithBatContext(batContext,[lifecycle=std::move(lifecycle),
      launch,cwd,runWithCwd]()mutable{
    auto invoke=[&](){launch(std::move(lifecycle));};
    if(cwd){
      need(runWithCwd,"runWithCwdOverride");
      runWithCwd(*cwd,invoke);
    }else invoke();
  });
  bool canReadOutputFile=false;
  for(const auto* gadget:context.query.gadgets){
    if(gadget && (gadgets::gadgetMatchesName(*gadget,services.fileReadGadgetName) ||
                gadgets::gadgetMatchesName(*gadget,services.bashGadgetName))){
      canReadOutputFile=true;break;
    }
  }
  return Json{{"data",{{"isAsync",true},{"status","async_launched"},
    {"agentId",missionId},{"description",registration.description},
    {"prompt",registration.poetry},
    {"outputFile",services.getMissionOutputPath(missionId)},
    {"canReadOutputFile",canReadOutputFile}}}};
}
} // namespace batbox::bats
