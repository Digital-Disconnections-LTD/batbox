#include "bats/BatGadgetLaunch.hpp"
#include <stdexcept>

namespace batbox::bats {
namespace {
const Json& field(const Json& object,const char* key){return value::property(object,key);}
std::string text(const Json& value){return value.is_string()?value.get<std::string>():"";}
template<class F>void need(const F& fn,const char* name){
  if(!fn)throw std::invalid_argument(std::string("BatGadget launch requires ")+name);
}
} // namespace
BatGadgetLaunchPlan prepareBatGadgetLaunch(const Json& input,
    const BatGadgetPreflightResult& preflight,const Json& assistantMessage,
    const BatContext& context,
    const std::function<std::future<Json>(const Json&)>& canUseGadget,
    BatGadgetLaunchServices& services){
  BatGadgetLaunchPlan out;
  const auto& selected=preflight.selectedBat;
  const auto& options=field(context.query.data,"options");
  const auto& appState=preflight.initialAppState;
  const auto& poetry=text(field(input,"prompt"));
  const auto& description=text(field(input,"description"));
  need(services.getBatModel,"getAgentModel");
  out.resolvedBatModel=services.getBatModel(field(selected,"model"),
    field(options,"mainLoopModel"),preflight.isForkPath?Json():preflight.modelParam,
    preflight.permissionMode);
  const Json& explicitIsolation=field(input,"isolation");
  const Json effectiveIsolation=!explicitIsolation.is_null()?
    explicitIsolation:field(selected,"isolation");
  const auto poetryEntries=prepareBatGadgetPoetryEntries(preflight,poetry,assistantMessage,
    context,services.poetry,out.resolvedBatModel);
  need(services.isBuiltInBat,"isBuiltInAgent");
  const bool builtIn=services.isBuiltInBat(selected);
  need(services.isBackgroundMissionsDisabled,"isBackgroundTasksDisabled");
  const bool backgroundRequested=field(input,"run_in_background")==true ||
    field(selected,"background")==true;
  out.metadata={{"prompt",poetry},{"resolvedAgentModel",out.resolvedBatModel},
    {"isBuiltInAgent",builtIn},{"startTime",preflight.startTime},
    {"agentType",field(selected,"agentType")},
    {"isAsync",backgroundRequested && !services.isBackgroundMissionsDisabled()}};
  need(services.feature,"feature");
  const bool isCoordinator=services.feature("COORDINATOR_MODE")?
    (need(services.coordinatorEnvironmentEnabled,"coordinatorEnvironmentEnabled"),
      services.coordinatorEnvironmentEnabled()):false;
  out.isCoordinator=isCoordinator;
  need(services.isForkSubagentEnabled,"isForkSubagentEnabled");
  const bool forceAsync=services.isForkSubagentEnabled();
  const bool assistantForceAsync=services.feature("KAIROS")?
    value::truthy(field(appState,"kairosEnabled")):false;
  out.shouldRunAsync=(backgroundRequested || isCoordinator || forceAsync ||
    assistantForceAsync || (services.isProactiveActive && services.isProactiveActive())) &&
    !services.isBackgroundMissionsDisabled();
  Json workerPermission=field(appState,"toolPermissionContext");
  const Json& selectedMode=field(selected,"permissionMode");
  workerPermission["mode"]=selectedMode.is_null()?Json("acceptEdits"):selectedMode;
  need(services.assembleGadgetPool,"assembleToolPool");
  const gadgets::Gadgets workerGadgets=services.assembleGadgetPool(workerPermission,
    field(field(appState,"mcp"),"tools"));
  need(services.createBatId,"createAgentId");
  out.earlyBatId=services.createBatId();
  if(effectiveIsolation=="worktree"){
    need(services.createBatWorktree,"createAgentWorktree");
    out.worktree.worktreeInfo=services.createBatWorktree(
      "agent-"+out.earlyBatId.substr(0,8));
    if(out.worktree.worktreeInfo->is_null())out.worktree.worktreeInfo.reset();
  }
  out.worktree.earlyBatId=out.earlyBatId;
  out.worktree.selectedBat=selected;
  out.worktree.description=description;
  Json poetryMessages=poetryEntries.poetryMessages;
  if(preflight.isForkPath && out.worktree.worktreeInfo){
    need(services.getCwd,"getCwd");
    need(services.buildWorktreeNotice,"buildWorktreeNotice");
    need(services.createUserMessage,"createUserMessage");
    poetryMessages.push_back(services.createUserMessage(Json{{"content",
      services.buildWorktreeNotice(services.getCwd(),
        text(field(*out.worktree.worktreeInfo,"worktreePath")))}}));
  }
  RunBatParams run;
  run.batDefinition=selected;
  run.poetryMessages=poetryMessages;
  run.gadgetUseContext=context;
  run.canUseGadget=canUseGadget;
  run.isAsync=out.shouldRunAsync;
  const Json& parentQuerySource=field(options,"querySource");
  if(!parentQuerySource.is_null())run.querySource=text(parentQuerySource);
  else{
    need(services.getQuerySourceForBat,"getQuerySourceForAgent");
    run.querySource=services.getQuerySourceForBat(text(field(selected,"agentType")),
      services.isBuiltInBat(selected));
  }
  if(!preflight.isForkPath && preflight.modelParam.is_string())
    run.model=preflight.modelParam.get<std::string>();
  if(preflight.isForkPath)
    run.override=Json{{"systemPrompt",poetryEntries.forkParentSystemPoetry}};
  else if(value::truthy(poetryEntries.enhancedSystemPoetry) &&
          !out.worktree.worktreeInfo && !value::truthy(field(input,"cwd"))){
    need(services.asSystemPoetry,"asSystemPrompt");
    run.override=Json{{"systemPrompt",services.asSystemPoetry(
      poetryEntries.enhancedSystemPoetry)}};
  }
  run.availableGadgets=preflight.isForkPath?context.query.gadgets:workerGadgets;
  if(preflight.isForkPath){
    run.forkContextMessages=field(context.query.data,"messages");
    run.useExactGadgets=true;
  }
  if(out.worktree.worktreeInfo)
    run.worktreePath=text(field(*out.worktree.worktreeInfo,"worktreePath"));
  if(input.contains("description"))run.description=description;
  const Json& cwd=field(input,"cwd");
  if(!cwd.is_null())out.cwdOverridePath=text(cwd);
  else if(run.worktreePath)out.cwdOverridePath=run.worktreePath;
  out.rootSetAppState=context.setAppStateForMissions?
    context.setAppStateForMissions:context.setAppState;
  out.runBatParams=std::move(run);
  return out;
}
} // namespace batbox::bats
