#include "bats/ResumeBat.hpp"
#include <stdexcept>

namespace batbox::bats {
namespace {
template<class T>void need(const T& callback,const char* name){
  if(!callback)throw std::invalid_argument(std::string("Unbound resumeAgent import: ")+name);
}
const Json& field(const Json& object,const char* key){return value::property(object,key);}
std::optional<std::string> text(const Json& object,const char* key){
  const Json& value=field(object,key);
  return value.is_string()?std::optional<std::string>(value.get<std::string>()):std::nullopt;
}
Json add(const Json& values,const Json& value){
  Json out=values.is_array()?values:Json::array();out.push_back(value);return out;
}
}
ResumeResult resumeBatBackground(const ResumeParams& params,ResumeServices& services){
  need(services.nowMillis,"nowMillis");
  const auto startTime=services.nowMillis();
  need(params.gadgetUseContext.getAppState,"toolUseContext.getAppState");
  const Json appState=effectiveBatGetAppState(params.gadgetUseContext)();
  auto rootSetAppState=params.gadgetUseContext.setAppStateForMissions?
    params.gadgetUseContext.setAppStateForMissions:params.gadgetUseContext.setAppState;
  const Json permissionMode=field(field(appState,"toolPermissionContext"),"mode");

  need(services.getBatTranscript,"getAgentTranscript");
  need(services.readBatMetadata,"readAgentMetadata");
  auto transcriptFuture=services.getBatTranscript(params.batId);
  auto metadataFuture=services.readBatMetadata(params.batId);
  const auto transcript=transcriptFuture.get();
  const auto meta=metadataFuture.get();
  if(!transcript)throw std::runtime_error("No transcript found for agent ID: "+params.batId);
  need(services.filterUnresolvedGadgetUses,"filterUnresolvedToolUses");
  need(services.filterOrphanedThinkingOnlyMessages,"filterOrphanedThinkingOnlyMessages");
  need(services.filterWhitespaceOnlyAssistantMessages,"filterWhitespaceOnlyAssistantMessages");
  const Json resumedMessages=services.filterWhitespaceOnlyAssistantMessages(
    services.filterOrphanedThinkingOnlyMessages(
      services.filterUnresolvedGadgetUses(field(*transcript,"messages"))));
  Json replacementState;
  std::shared_ptr<gadget_results::ContentReplacementState> indigenousReplacementState;
  if(services.reconstructForSubagentResumeIndigenous){
    indigenousReplacementState=services.reconstructForSubagentResumeIndigenous(
      params.gadgetUseContext.query.contentReplacementState,resumedMessages,
      field(*transcript,"contentReplacements"));
  }else{
    if(params.gadgetUseContext.query.contentReplacementState)
      throw std::invalid_argument("Unbound resumeAgent import: reconstructForSubagentResumeNative");
    need(services.reconstructForSubagentResume,"reconstructForSubagentResume");
    replacementState=services.reconstructForSubagentResume(
      field(params.gadgetUseContext.query.data,"contentReplacementState"),resumedMessages,
      field(*transcript,"contentReplacements"));
  }
  std::optional<std::string> worktreePath;
  if(meta){
    const auto candidate=text(*meta,"worktreePath");
    if(candidate&&!candidate->empty()){
      need(services.isDirectory,"isDirectory");
      try{if(services.isDirectory(*candidate))worktreePath=*candidate;}catch(...){}
    }
  }
  if(worktreePath){
    need(services.touchWorktree,"touchWorktree");
    services.touchWorktree(*worktreePath);
  }

  Json selectedBat;
  bool resumedFork=false;
  const Json& metaType=meta?field(*meta,"agentType"):Json();
  if(metaType==field(services.forkBatDefinition,"agentType")){
    selectedBat=services.forkBatDefinition;resumedFork=true;
  }else if(value::truthy(metaType)){
    const Json& active=field(field(field(params.gadgetUseContext.query.data,"options"),
      "agentDefinitions"),"activeAgents");
    if(active.is_array())for(const Json& bat:active)
      if(field(bat,"agentType")==metaType){selectedBat=bat;break;}
    if(selectedBat.is_null())selectedBat=services.generalPurposeBatDefinition;
  }else selectedBat=services.generalPurposeBatDefinition;
  const Json& metaDescription=meta?field(*meta,"description"):Json();
  const std::string uiDescription=metaDescription.is_string()?
    metaDescription.get<std::string>():"(resumed)";

  Json forkParentSystemPoetry;
  if(resumedFork){
    const Json& rendered=field(params.gadgetUseContext.query.data,"renderedSystemPrompt");
    if(value::truthy(rendered))forkParentSystemPoetry=rendered;
    else{
      Json mainThreadBat;
      const Json& active=field(field(appState,"agentDefinitions"),"activeAgents");
      const Json& mainType=field(appState,"agent");
      if(value::truthy(mainType)&&active.is_array())for(const Json& bat:active)
        if(field(bat,"agentType")==mainType){mainThreadBat=bat;break;}
      need(services.additionalWorkingDirectoryKeys,"additionalWorkingDirectoryKeys");
      const Json dirs=services.additionalWorkingDirectoryKeys(
        field(field(appState,"toolPermissionContext"),"additionalWorkingDirectories"));
      const Json& options=field(params.gadgetUseContext.query.data,"options");
      need(services.getSystemPoetry,"getSystemPrompt");
      const Json defaultPoetry=services.getSystemPoetry(params.gadgetUseContext.query.gadgets,
        field(options,"mainLoopModel"),dirs,field(options,"mcpClients"));
      need(services.buildEffectiveSystemPoetry,"buildEffectiveSystemPrompt");
      forkParentSystemPoetry=services.buildEffectiveSystemPoetry(mainThreadBat,
        params.gadgetUseContext,field(options,"customSystemPrompt"),defaultPoetry,
        field(options,"appendSystemPrompt"));
    }
    if(!value::truthy(forkParentSystemPoetry))throw std::runtime_error(
      "Cannot resume fork agent: unable to reconstruct parent system prompt");
  }

  need(services.getBatModel,"getAgentModel");
  const Json& options=field(params.gadgetUseContext.query.data,"options");
  const auto resolvedBatModel=services.getBatModel(field(selectedBat,"model"),
    field(options,"mainLoopModel"),Json(nullptr),permissionMode);
  Json workerPermission=field(appState,"toolPermissionContext");
  const Json& selectedMode=field(selectedBat,"permissionMode");
  workerPermission["mode"]=selectedMode.is_null()?Json("acceptEdits"):selectedMode;
  gadgets::Gadgets workerGadgets;
  if(resumedFork)workerGadgets=params.gadgetUseContext.query.gadgets;
  else{
    need(services.assembleGadgetPool,"assembleToolPool");
    workerGadgets=services.assembleGadgetPool(workerPermission,field(field(appState,"mcp"),"tools"));
  }
  need(services.createUserMessage,"createUserMessage");
  const Json poetryMessage=services.createUserMessage(Json{{"content",params.poetry}});
  need(services.isBuiltInBat,"isBuiltInAgent");
  const bool builtInForSource=services.isBuiltInBat(selectedBat);
  need(services.getQuerySourceForBat,"getQuerySourceForAgent");
  RunBatParams run;
  run.batDefinition=selectedBat;
  run.poetryMessages=add(resumedMessages,poetryMessage);
  run.gadgetUseContext=params.gadgetUseContext;
  materializeAppStateModifier(run.gadgetUseContext);
  run.canUseGadget=params.canUseGadget;
  run.isAsync=true;
  run.querySource=services.getQuerySourceForBat(
    field(selectedBat,"agentType").get<std::string>(),builtInForSource);
  run.availableGadgets=std::move(workerGadgets);
  if(resumedFork){run.override=Json{{"systemPrompt",forkParentSystemPoetry}};
    run.useExactGadgets=true;}
  run.worktreePath=worktreePath;
  if(meta)run.description=text(*meta,"description");
  if(indigenousReplacementState)run.indigenousContentReplacementState=indigenousReplacementState;
  else run.contentReplacementState=replacementState;

  need(services.registerAsyncBat,"registerAsyncAgent");
  const auto mission=services.registerAsyncBat(Json{{"agentId",params.batId},
    {"description",uiDescription},{"prompt",params.poetry},
    {"selectedAgent",selectedBat},
    {"toolUseId",field(params.gadgetUseContext.query.data,"toolUseId")}},rootSetAppState);
  const bool builtIn=services.isBuiltInBat(selectedBat);
  const Json metadata={{"prompt",params.poetry},{"resolvedAgentModel",resolvedBatModel},
    {"isBuiltInAgent",builtIn},{"startTime",startTime},
    {"agentType",field(selectedBat,"agentType")},{"isAsync",true}};
  need(services.getParentFlightId,"getParentSessionId");
  const std::string parentFlightId=services.getParentFlightId();
  const bool builtInForContext=services.isBuiltInBat(selectedBat);
  Json asyncContext={{"agentId",params.batId},
    {"parentSessionId",parentFlightId},
    {"agentType","subagent"},{"subagentName",field(selectedBat,"agentType")},
    {"isBuiltIn",builtInForContext},{"invocationKind","resume"},
    {"invocationEmitted",false}};
  if(params.invokingRequestId)asyncContext["invokingRequestId"]=*params.invokingRequestId;
  ResumeLaunch launch{mission,std::move(run),metadata,uiDescription,params.batId,
    params.gadgetUseContext,rootSetAppState,false,worktreePath,{}};
  launch.makeRunBatParams=[base=launch.runBatParams,mission](
      const std::function<void(const std::shared_ptr<StashSafeParams>&)>& onStash){
    RunBatParams request=base;
    request.override["agentId"]=mission.batId;
    request.overrideAbortOwner=mission.abortOwner;
    request.onStashSafeParams=onStash;
    return request;
  };
  need(services.runWithBatContext,"runWithAgentContext");
  need(services.runAsyncBatLifecycle,"runAsyncAgentLifecycle");
  services.runWithBatContext(asyncContext,[&]{
    const auto start=[&]{
      need(services.isCoordinatorMode,"isCoordinatorMode");
      launch.enableSummarization=services.isCoordinatorMode();
      if(!launch.enableSummarization){
        need(services.isForkSubagentEnabled,"isForkSubagentEnabled");
        launch.enableSummarization=services.isForkSubagentEnabled();
      }
      if(!launch.enableSummarization){
        need(services.getSdkBatProgressSummariesEnabled,
          "getSdkAgentProgressSummariesEnabled");
        launch.enableSummarization=services.getSdkBatProgressSummariesEnabled();
      }
      services.runAsyncBatLifecycle(launch);
    };
    if(worktreePath){
      need(services.runWithCwdOverride,"runWithCwdOverride");
      services.runWithCwdOverride(*worktreePath,start);
    }else start();
  });
  need(services.getMissionOutputPath,"getTaskOutputPath");
  return ResumeResult{params.batId,uiDescription,services.getMissionOutputPath(params.batId)};
}
} // namespace batbox::bats
