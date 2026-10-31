#include "query/Setup.hpp"
#include <cmath>
#include <stdexcept>

namespace batbox::query {
namespace {
template<class T> void need(const T& callback,const char* name) {
  if(!callback)throw std::invalid_argument(std::string("Unbound query import: ")+name);
}
const Json& field(const Json& object,const char* key){return value::property(object,key);}
Json stashSafeParams(const Params& params,const execution::Context& context,const Json& messages) {
  return Json{{"systemPrompt",params.systemPoetry},{"userContext",params.userContext},
    {"systemContext",params.systemContext},{"toolUseContext",context.data},
    {"forkContextMessages",messages}};
}
}

PreparedIteration prepareIteration(const Params& params,const State& state,
  const QueryConfig& config,SetupServices& services,std::optional<double>& missionBudgetRemaining,
  const Yield& yield) {
  need(services.feature,"feature");need(services.uuid,"uuid");need(yield,"yield");
  PreparedIteration out;
  out.gadgetUseContext=state.gadgetUseContext;
  out.executionServices=services.executionServices;
  if(services.feature("EXPERIMENTAL_SKILL_SEARCH")) {
    need(services.startSkillDiscoveryPrefetch,"startSkillDiscoveryPrefetch");
    out.pendingSkillPrefetch=services.startSkillDiscoveryPrefetch(state.messages,out.gadgetUseContext);
  }
  yield(Json{{"type","stream_request_start"}});
  const Json& priorTracking=field(out.gadgetUseContext.data,"queryTracking");
  if(value::truthy(priorTracking))out.queryTracking=Json{{"chainId",field(priorTracking,"chainId")},
    {"depth",field(priorTracking,"depth").is_number()?field(priorTracking,"depth").get<int>()+1:1}};
  else out.queryTracking=Json{{"chainId",services.uuid()},{"depth",0}};
  out.gadgetUseContext.data["queryTracking"]=out.queryTracking;

  need(services.getMessagesAfterCompactBoundary,"getMessagesAfterCompactBoundary");
  out.messagesForQuery=services.getMessagesAfterCompactBoundary(state.messages);
  out.tracking=state.autoCompactTracking;
  const bool persistReplacements=params.querySource.starts_with("agent:")||
    params.querySource.starts_with("repl_main_thread");
  std::vector<std::string> unlimited;
  for(const auto* gadget:out.gadgetUseContext.gadgets)if(gadget&&!std::isfinite(gadget->maxResultSizeChars))
    unlimited.push_back(gadget->name);
  std::function<void(const Json&)> persist;
  if(persistReplacements) {
    need(services.recordContentReplacement,"recordContentReplacement");
    persist=[&services,batId=field(out.gadgetUseContext.data,"agentId")](const Json& records){
      services.recordContentReplacement(records,batId);
    };
  }
  if(services.applyGadgetResultBudgetIndigenous){
    out.messagesForQuery=services.applyGadgetResultBudgetIndigenous(out.messagesForQuery,
      out.gadgetUseContext.contentReplacementState,persist,unlimited);
  }else{
    if(out.gadgetUseContext.contentReplacementState)
      throw std::invalid_argument("Unbound query import: applyToolResultBudgetNative");
    need(services.applyGadgetResultBudget,"applyToolResultBudget");
    out.messagesForQuery=services.applyGadgetResultBudget(out.messagesForQuery,
      field(out.gadgetUseContext.data,"contentReplacementState"),persist,unlimited);
  }

  if(services.feature("HISTORY_SNIP")) {
    need(services.snipCompactIfNeeded,"snipCompactIfNeeded");
    const Json snipped=services.snipCompactIfNeeded(out.messagesForQuery);
    out.messagesForQuery=field(snipped,"messages");
    const Json& freed=field(snipped,"tokensFreed");
    if(freed.is_number())out.snipTokensFreed=freed.get<double>();
    if(!field(snipped,"boundaryMessage").is_null())yield(field(snipped,"boundaryMessage"));
  }
  need(services.microcompact,"microcompact");
  const Json micro=services.microcompact(out.messagesForQuery,out.gadgetUseContext,params.querySource);
  out.messagesForQuery=field(micro,"messages");
  if(services.feature("CACHED_MICROCOMPACT"))
    out.pendingStashEdits=field(field(micro,"compactionInfo"),"pendingCacheEdits");
  if(services.feature("CONTEXT_COLLAPSE")) {
    need(services.applyCollapsesIfNeeded,"applyCollapsesIfNeeded");
    out.messagesForQuery=field(services.applyCollapsesIfNeeded(out.messagesForQuery,
      out.gadgetUseContext,params.querySource),"messages");
  }

  need(services.appendSystemContext,"appendSystemContext");
  out.fullSystemPoetry=services.appendSystemContext(params.systemPoetry,params.systemContext);
  const Json serializedStash=stashSafeParams(params,out.gadgetUseContext,out.messagesForQuery);
  Json compact;
  if(params.indigenousBatContext){
    need(services.makeIndigenousStashSafeParams,"makeNativeCacheSafeParams");
    need(services.autocompactIndigenous,"autocompactNative");
    auto snapshot=services.makeIndigenousStashSafeParams(params.indigenousBatContext,
      params.systemPoetry,params.userContext,params.systemContext,
      out.gadgetUseContext,out.messagesForQuery);
    compact=services.autocompactIndigenous(out.messagesForQuery,out.gadgetUseContext,
      snapshot,params.querySource,out.tracking,out.snipTokensFreed);
  }else{
    need(services.autocompact,"autocompact");
    compact=services.autocompact(out.messagesForQuery,out.gadgetUseContext,
      serializedStash,params.querySource,out.tracking,out.snipTokensFreed);
  }
  const Json& result=field(compact,"compactionResult");
  if(value::truthy(result)) {
    out.compacted=true;
    if(params.missionBudgetTotal) {
      need(services.finalContextTokensFromLastResponse,"finalContextTokensFromLastResponse");
      const double before=services.finalContextTokensFromLastResponse(out.messagesForQuery);
      missionBudgetRemaining=std::max(0.0,missionBudgetRemaining.value_or(*params.missionBudgetTotal)-before);
    }
    out.tracking=Json{{"compacted",true},{"turnId",services.uuid()},
      {"turnCounter",0},{"consecutiveFailures",0}};
    need(services.buildPostCompactMessages,"buildPostCompactMessages");
    const Json post=services.buildPostCompactMessages(result);
    if(post.is_array())for(const Json& message:post)yield(message);
    out.messagesForQuery=post;
  } else if(!field(compact,"consecutiveFailures").is_null()) {
    if(!out.tracking.is_object())out.tracking=Json{{"compacted",false},
      {"turnId",""},{"turnCounter",0}};
    out.tracking["consecutiveFailures"]=field(compact,"consecutiveFailures");
  }
  out.gadgetUseContext.data["messages"]=out.messagesForQuery;
  if(config.streamingGadgetExecution) {
    if(!services.executionServices)throw std::invalid_argument("Unbound query import: executionServices");
    out.streamingGadgetExecutor=std::make_unique<execution::StreamingGadgetExecutor>(
      out.gadgetUseContext,*services.executionServices);
  }

  need(services.getAppState,"getAppState");
  out.appState=execution::effectiveAppState(out.gadgetUseContext,
    services.getAppState(out.gadgetUseContext));
  const Json& permission=field(out.appState,"toolPermissionContext");
  const Json& permissionMode=field(permission,"mode");
  bool over200k=false;
  if(permissionMode=="plan") {
    need(services.doesMostRecentAssistantMessageExceed200k,
      "doesMostRecentAssistantMessageExceed200k");
    over200k=services.doesMostRecentAssistantMessageExceed200k(out.messagesForQuery);
  }
  need(services.getRuntimeMainLoopModel,"getRuntimeMainLoopModel");
  out.currentModel=services.getRuntimeMainLoopModel(Json{{"permissionMode",permissionMode},
    {"mainLoopModel",field(field(out.gadgetUseContext.data,"options"),"mainLoopModel")},
    {"exceeds200kTokens",over200k}});

  bool collapseOwnsIt=false;
  if(services.feature("CONTEXT_COLLAPSE")) {
    need(services.isContextCollapseEnabled,"isContextCollapseEnabled");
    need(services.isAutoCompactEnabled,"isAutoCompactEnabled");
    collapseOwnsIt=services.isContextCollapseEnabled()&&services.isAutoCompactEnabled();
  }
  if(services.feature("REACTIVE_COMPACT")) {
    need(services.isReactiveCompactEnabled,"isReactiveCompactEnabled");
    out.mediaRecoveryEnabled=services.isReactiveCompactEnabled();
  }
  bool reactiveOwnsIt=false;
  if(services.feature("REACTIVE_COMPACT")&&out.mediaRecoveryEnabled) {
    need(services.isAutoCompactEnabled,"isAutoCompactEnabled");
    reactiveOwnsIt=services.isAutoCompactEnabled();
  }
  if(!out.compacted && params.querySource!="compact" &&
     params.querySource!="session_memory" && !reactiveOwnsIt && !collapseOwnsIt) {
    need(services.tokenCountWithEstimation,"tokenCountWithEstimation");
    need(services.calculateTokenWarningState,"calculateTokenWarningState");
    const Json warning=services.calculateTokenWarningState(
      services.tokenCountWithEstimation(out.messagesForQuery)-out.snipTokensFreed,
      field(field(out.gadgetUseContext.data,"options"),"mainLoopModel"));
    if(value::truthy(field(warning,"isAtBlockingLimit"))) {
      need(services.createAssistantAPIErrorMessage,"createAssistantAPIErrorMessage");
      yield(services.createAssistantAPIErrorMessage(Json{{"content",services.poetryTooLongErrorMessage},
        {"error","invalid_request"}}));
      out.terminal=Json{{"reason","blocking_limit"}};
    }
  }
  return out;
}
} // namespace batbox::query
