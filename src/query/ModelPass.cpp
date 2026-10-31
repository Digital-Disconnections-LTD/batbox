#include "query/ModelPass.hpp"
#include "retry/Retry.hpp"
#include <stdexcept>

namespace batbox::query {
namespace {
template<class T> void need(const T& callback,const char* name){
  if(!callback)throw std::invalid_argument(std::string("Unbound query import: ")+name);
}
const Json& field(const Json& object,const char* name){return value::property(object,name);}
std::string stringField(const Json& object,const char* name){
  const Json& value=field(object,name);return value.is_string()?value.get<std::string>():std::string{};
}
bool pendingMcp(const Json& appState){
  const Json& clients=field(field(appState,"mcp"),"clients");
  if(clients.is_array())for(const Json& client:clients)if(field(client,"type")=="pending")return true;
  return false;
}
void syncAssistants(ModelPassResult& result){
  for(std::size_t i=0;i<result.assistantRefs.size()&&i<result.assistantMessages.size();++i)
    result.assistantMessages[i]=*result.assistantRefs[i];
}
void collectNormalizedUser(const Json& message,const execution::Context& context,
  ModelPassResult& result,ModelServices& services){
  need(services.normalizeMessagesForAPI,"normalizeMessagesForAPI");
  const Json normalized=services.normalizeMessagesForAPI(Json::array({message}),context.gadgets);
  if(normalized.is_array())for(const Json& item:normalized)
    if(field(item,"type")=="user")result.gadgetResults.push_back(item);
}
model::MessageRef cloneForYield(const model::MessageRef& messageRef,
  const execution::Context& context){
  const Json& message=*messageRef;
  if(field(message,"type")!="assistant")return messageRef;
  const Json& content=field(field(message,"message"),"content");
  if(!content.is_array())return messageRef;
  model::MessageRef clone;
  bool changed=false;
  for(std::size_t i=0;i<content.size();++i){
    const Json& block=content[i];
    const Json& input=field(block,"input");
    if(field(block,"type")!="tool_use"||!input.is_object())continue;
    const auto* gadget=gadgets::findGadgetByName(context.gadgets,stringField(block,"name"));
    if(!gadget||!gadget->backfillObservableInput)continue;
    Json copy=input;
    gadget->backfillObservableInput(copy);
    bool added=false;
    for(const auto& [key,value]:copy.items())if(!input.contains(key))added=true;
    if(added){
      if(!clone)clone=std::make_shared<Json>(message);
      (*clone)["message"]["content"][i]["input"]=copy;
      changed=true;
    }
  }
  return changed?clone:messageRef;
}
ModelRequest makeRequest(const Params& params,const State& state,const PreparedIteration& prepared,
  const QueryConfig& config,std::optional<double> missionBudgetRemaining,ModelServices& services){
  need(services.prependUserContext,"prependUserContext");
  ModelRequest request;
  request.messages=services.prependUserContext(prepared.messagesForQuery,params.userContext);
  request.systemPoetry=prepared.fullSystemPoetry;
  const Json& options=field(prepared.gadgetUseContext.data,"options");
  request.thinkingConfig=field(options,"thinkingConfig");
  request.gadgets=prepared.gadgetUseContext.gadgets;
  request.signal=prepared.gadgetUseContext.stop;
  request.getGadgetPermissionContext=[&services,&prepared]{
    need(services.getAppState,"getAppState");
    return field(execution::effectiveAppState(prepared.gadgetUseContext,
      services.getAppState(prepared.gadgetUseContext)),"toolPermissionContext");
  };
  request.addNotification=services.addNotification;
  Json modelOptions={{"model",prepared.currentModel},
    {"isNonInteractiveSession",field(options,"isNonInteractiveSession")},
    {"querySource",params.querySource},
    {"agents",field(field(options,"agentDefinitions"),"activeAgents")},
    {"allowedAgentTypes",field(field(options,"agentDefinitions"),"allowedAgentTypes")},
    {"hasAppendSystemPrompt",value::truthy(field(options,"appendSystemPrompt"))},
    {"maxOutputTokensOverride",state.maxOutputTokensOverride?Json(*state.maxOutputTokensOverride):Json()},
    {"mcpTools",field(field(prepared.appState,"mcp"),"tools")},
    {"hasPendingMcpServers",pendingMcp(prepared.appState)},
    {"queryTracking",prepared.queryTracking},
    {"effortValue",field(prepared.appState,"effortValue")},
    {"advisorModel",field(prepared.appState,"advisorModel")},
    {"skipCacheWrite",params.skipStashWrite},
    {"agentId",field(prepared.gadgetUseContext.data,"agentId")}};
  if(params.fallbackModel)modelOptions["fallbackModel"]=*params.fallbackModel;
  if(config.fastModeEnabled)modelOptions["fastMode"]=field(prepared.appState,"fastMode");
  if(params.missionBudgetTotal){
    modelOptions["taskBudget"]={{"total",*params.missionBudgetTotal}};
    if(missionBudgetRemaining)modelOptions["taskBudget"]["remaining"]=*missionBudgetRemaining;
  }
  request.options=std::move(modelOptions);
  return request;
}
}

ModelPassResult runModelPass(const Params& params,const State& state,
  PreparedIteration& prepared,const QueryConfig& config,
  std::optional<double> missionBudgetRemaining,ModelServices& services,const Yield& yield){
  need(services.callModel,"callModel");need(services.feature,"feature");need(yield,"yield");
  ModelPassResult result;
  bool attemptWithFallback=true;
  try {
    while(attemptWithFallback){
      attemptWithFallback=false;
      try {
        bool streamingFallbackOccurred=false;
        const ModelRequest request=makeRequest(params,state,prepared,config,missionBudgetRemaining,services);
        auto stream=services.callModel(request,[&]{streamingFallbackOccurred=true;});
        need(stream,"callModel stream");
        while(auto messageRef=stream()){
          const Json& message=**messageRef;
          if(streamingFallbackOccurred){
            syncAssistants(result);
            for(const Json& orphan:result.assistantMessages)
              yield(Json{{"type","tombstone"},{"message",orphan}});
            result.assistantMessages=Json::array();
            result.assistantRefs.clear();
            result.gadgetResults=Json::array();
            result.gadgetUseBlocks=Json::array();
            result.needsFollowUp=false;
            if(prepared.streamingGadgetExecutor){
              prepared.streamingGadgetExecutor->discard();
              prepared.discardedExecutors.push_back(std::move(prepared.streamingGadgetExecutor));
              prepared.streamingGadgetExecutor=std::make_unique<execution::StreamingGadgetExecutor>(
                prepared.gadgetUseContext,*prepared.executionServices);
            }
          }
          const model::MessageRef emitted=cloneForYield(*messageRef,prepared.gadgetUseContext);
          bool withheld=false;
          if(services.feature("CONTEXT_COLLAPSE")){
            need(services.collapseWithholdsPoetryTooLong,"collapseWithholdsPromptTooLong");
            if(services.collapseWithholdsPoetryTooLong(message))withheld=true;
          }
          if(services.feature("REACTIVE_COMPACT")){
            need(services.reactiveWithholdsPoetryTooLong,"reactiveWithholdsPromptTooLong");
            if(services.reactiveWithholdsPoetryTooLong(message))withheld=true;
            if(prepared.mediaRecoveryEnabled){
              need(services.reactiveWithholdsMediaSizeError,"reactiveWithholdsMediaSizeError");
              if(services.reactiveWithholdsMediaSizeError(message))withheld=true;
            }
          }
          if(isWithheldMaxOutputTokens(message))withheld=true;
          if(!withheld)yield(emitted);
          if(field(message,"type")=="assistant"){
            result.assistantMessages.push_back(message);
            result.assistantRefs.push_back(*messageRef);
            const Json& content=field(field(message,"message"),"content");
            if(content.is_array())for(const Json& block:content)
              if(field(block,"type")=="tool_use"){
                result.gadgetUseBlocks.push_back(block);
                result.needsFollowUp=true;
                if(prepared.streamingGadgetExecutor&&!prepared.gadgetUseContext.stop.stop_requested())
                  prepared.streamingGadgetExecutor->addGadget(block,message);
              }
          }
          if(prepared.streamingGadgetExecutor&&!prepared.gadgetUseContext.stop.stop_requested())
            for(const auto& update:prepared.streamingGadgetExecutor->getCompletedResults())
              if(update.message){yield(*update.message);collectNormalizedUser(*update.message,
                prepared.gadgetUseContext,result,services);}
        }
        if(services.feature("CACHED_MICROCOMPACT")&&!prepared.pendingStashEdits.is_null()){
          syncAssistants(result);
          const Json& last=result.assistantMessages.empty()?Json():result.assistantMessages.back();
          const Json& cumulative=field(field(field(last,"message"),"usage"),"cache_deleted_input_tokens");
          const Json& baseline=field(prepared.pendingStashEdits,"baselineCacheDeletedTokens");
          const double deleted=std::max(0.0,(cumulative.is_number()?cumulative.get<double>():0.0)-
            (baseline.is_number()?baseline.get<double>():0.0));
          if(deleted>0){need(services.createMicrocompactBoundaryMessage,
            "createMicrocompactBoundaryMessage");
            yield(services.createMicrocompactBoundaryMessage(
              field(prepared.pendingStashEdits,"trigger"),0,deleted,
              field(prepared.pendingStashEdits,"deletedToolIds"),Json::array()));}
        }
      }catch(const retry::FallbackTriggeredError& fallback){
        if(!params.fallbackModel)throw;
        prepared.currentModel=*params.fallbackModel;
        attemptWithFallback=true;
        need(services.createUserMessage,"createUserMessage");
        syncAssistants(result);
        yieldMissingGadgetResultBlocks(result.assistantMessages,"Model fallback triggered",
          services.createUserMessage,yield);
        result.assistantMessages=Json::array();result.assistantRefs.clear();result.gadgetResults=Json::array();
        result.gadgetUseBlocks=Json::array();result.needsFollowUp=false;
        if(prepared.streamingGadgetExecutor){
          prepared.streamingGadgetExecutor->discard();
          prepared.discardedExecutors.push_back(std::move(prepared.streamingGadgetExecutor));
          prepared.streamingGadgetExecutor=std::make_unique<execution::StreamingGadgetExecutor>(
            prepared.gadgetUseContext,*prepared.executionServices);
        }
        prepared.gadgetUseContext.data["options"]["mainLoopModel"]=*params.fallbackModel;
        if(config.internalUser){need(services.stripSignatureBlocks,"stripSignatureBlocks");
          prepared.messagesForQuery=services.stripSignatureBlocks(prepared.messagesForQuery);}
        need(services.createSystemMessage,"createSystemMessage");
        need(services.renderModelName,"renderModelName");
        yield(services.createSystemMessage("Switched to "+services.renderModelName(fallback.fallbackModel)+
          " due to high demand for "+services.renderModelName(fallback.originalModel),"warning"));
      }
    }
  }catch(const std::exception& error){
    syncAssistants(result);
    if(services.isImageSizeOrResizeError&&services.isImageSizeOrResizeError(error)){
      need(services.createAssistantAPIErrorMessage,"createAssistantAPIErrorMessage");
      yield(services.createAssistantAPIErrorMessage(Json{{"content",error.what()}}));
      result.terminal=Json{{"reason","image_error"}};return result;
    }
    need(services.createUserMessage,"createUserMessage");
    yieldMissingGadgetResultBlocks(result.assistantMessages,error.what(),
      services.createUserMessage,yield);
    need(services.createAssistantAPIErrorMessage,"createAssistantAPIErrorMessage");
    yield(services.createAssistantAPIErrorMessage(Json{{"content",error.what()}}));
    result.terminal=Json{{"reason","model_error"}};
    result.error=std::current_exception();
    return result;
  }
  syncAssistants(result);
  return result;
}
} // namespace batbox::query
