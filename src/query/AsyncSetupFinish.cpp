#include "query/AsyncSetupInternal.hpp"
#include <algorithm>

namespace batbox::query::async_setup {
void Run::afterAutocompact(const Json& compact){
  const Json& compacted=field(compact,"compactionResult");
  if(value::truthy(compacted)){
    out.compacted=true;
    if(params.missionBudgetTotal){
      need(services.sync.finalContextTokensFromLastResponse,
        "finalContextTokensFromLastResponse");
      const double before=services.sync.finalContextTokensFromLastResponse(out.messagesForQuery);
      remaining=std::max(0.0,remaining.value_or(*params.missionBudgetTotal)-before);
    }
    out.tracking={{"compacted",true},{"turnId",services.sync.uuid()},
      {"turnCounter",0},{"consecutiveFailures",0}};
    need(services.sync.buildPostCompactMessages,"buildPostCompactMessages");
    postMessages=services.sync.buildPostCompactMessages(compacted);
    if(!postMessages.is_array())
      throw std::invalid_argument("Post-compact messages must be an array");
    postIndex=0;
    stage=Stage::YieldPost;
  }else{
    if(!field(compact,"consecutiveFailures").is_null()){
      if(!out.tracking.is_object())out.tracking={{"compacted",false},
        {"turnId",""},{"turnCounter",0}};
      out.tracking["consecutiveFailures"]=field(compact,"consecutiveFailures");
    }
    stage=Stage::AfterPost;
  }
}

void Run::finishSetup(){
  out.gadgetUseContext.data["messages"]=out.messagesForQuery;
  services.queryCheckpoint("query_setup_start");
  if(config.streamingGadgetExecution){
    if(!services.sync.executionServices)
      throw std::invalid_argument("Unbound async query import: executionServices");
    out.streamingGadgetExecutor=std::make_unique<execution::StreamingGadgetExecutor>(
      out.gadgetUseContext,*services.sync.executionServices);
  }
  need(services.sync.getAppState,"getAppState");
  out.appState=execution::effectiveAppState(out.gadgetUseContext,
    services.sync.getAppState(out.gadgetUseContext));
  const Json& permission=field(out.appState,"toolPermissionContext");
  const Json& permissionMode=field(permission,"mode");
  bool over200k=false;
  if(permissionMode=="plan"){
    need(services.sync.doesMostRecentAssistantMessageExceed200k,
      "doesMostRecentAssistantMessageExceed200k");
    over200k=services.sync.doesMostRecentAssistantMessageExceed200k(out.messagesForQuery);
  }
  need(services.sync.getRuntimeMainLoopModel,"getRuntimeMainLoopModel");
  out.currentModel=services.sync.getRuntimeMainLoopModel(Json{{"permissionMode",permissionMode},
    {"mainLoopModel",field(field(out.gadgetUseContext.data,"options"),"mainLoopModel")},
    {"exceeds200kTokens",over200k}});
  services.queryCheckpoint("query_setup_end");

  bool collapseOwnsIt=false;
  if(services.sync.feature("CONTEXT_COLLAPSE") &&
     services.sync.isContextCollapseEnabled &&
     services.sync.isContextCollapseEnabled()){
    need(services.sync.isAutoCompactEnabled,"isAutoCompactEnabled");
    collapseOwnsIt=services.sync.isAutoCompactEnabled();
  }
  out.mediaRecoveryEnabled=services.sync.isReactiveCompactEnabled?
    services.sync.isReactiveCompactEnabled():false;
  const bool blockEligible=!out.compacted && params.querySource!="compact" &&
    params.querySource!="session_memory";
  bool reactiveOwnsIt=false;
  if(blockEligible && services.sync.isReactiveCompactEnabled &&
     services.sync.isReactiveCompactEnabled()){
    need(services.sync.isAutoCompactEnabled,"isAutoCompactEnabled");
    reactiveOwnsIt=services.sync.isAutoCompactEnabled();
  }
  if(blockEligible && !reactiveOwnsIt && !collapseOwnsIt){
    need(services.sync.tokenCountWithEstimation,"tokenCountWithEstimation");
    need(services.sync.calculateTokenWarningState,"calculateTokenWarningState");
    const Json warning=services.sync.calculateTokenWarningState(
      services.sync.tokenCountWithEstimation(out.messagesForQuery)-out.snipTokensFreed,
      field(field(out.gadgetUseContext.data,"options"),"mainLoopModel"));
    if(value::truthy(field(warning,"isAtBlockingLimit"))){
      need(services.sync.createAssistantAPIErrorMessage,"createAssistantAPIErrorMessage");
      out.terminal=Json{{"reason","blocking_limit"}};
      emit(services.sync.createAssistantAPIErrorMessage(Json{{"content",
        services.sync.poetryTooLongErrorMessage},{"error","invalid_request"}}),
        Stage::AfterBlocking);
      return;
    }
  }
  finish();
}
} // namespace batbox::query::async_setup
