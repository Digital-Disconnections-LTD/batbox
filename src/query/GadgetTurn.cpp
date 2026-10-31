#include "runtime/AsyncLaunch.hpp"
#include "query/GadgetTurn.hpp"
#include "query/MemoryAttachmentFilter.hpp"
#include <algorithm>
#include <future>
#include <stdexcept>

namespace batbox::query {
namespace {
template<class T>void need(const T& callback,const char* name){
  if(!callback)throw std::invalid_argument(std::string("Unbound query import: ")+name);
}
const Json& field(const Json& object,const char* name){return value::property(object,name);}
std::string stringField(const Json& object,const char* name){
  const Json& value=field(object,name);return value.is_string()?value.get<std::string>():std::string{};
}
Json concatMessages(const Json& first,const Json& second){
  Json result=first.is_array()?first:Json::array();
  if(second.is_array())for(const Json& message:second)result.push_back(message);
  return result;
}
void appendNormalizedUsers(const Json& message,const execution::Context& context,
  ModelPassResult& result,GadgetTurnServices& services){
  need(services.normalizeMessagesForAPI,"normalizeMessagesForAPI");
  const Json normalized=services.normalizeMessagesForAPI(Json::array({message}),context.gadgets);
  if(normalized.is_array())for(const Json& item:normalized)
    if(field(item,"type")=="user")result.gadgetResults.push_back(item);
}
Json summaryInfo(const ModelPassResult& modelResult){
  Json infos=Json::array();
  for(const Json& block:modelResult.gadgetUseBlocks){
    Json output;
    const Json& id=field(block,"id");
    for(const Json& result:modelResult.gadgetResults){
      if(field(result,"type")!="user")continue;
      const Json& content=field(field(result,"message"),"content");
      if(!content.is_array())continue;
      bool found=false;
      for(const Json& item:content)if(field(item,"type")=="tool_result"&&field(item,"tool_use_id")==id){
        output=field(item,"content");found=true;break;
      }
      if(found)break;
    }
    infos.push_back(Json{{"name",field(block,"name")},{"input",field(block,"input")},
      {"output",output}});
  }
  return infos;
}
std::optional<std::shared_future<Json>> startSummary(
  const PreparedIteration& prepared,const ModelPassResult& modelResult,
  const QueryConfig& config,GadgetTurnServices& services){
  if(!config.emitGadgetUseSummaries || modelResult.gadgetUseBlocks.empty() ||
     prepared.gadgetUseContext.stop.stop_requested() ||
     value::truthy(field(prepared.gadgetUseContext.data,"agentId")))return std::nullopt;
  need(services.generateGadgetUseSummary,"generateToolUseSummary");
  need(services.createGadgetUseSummaryMessage,"createToolUseSummaryMessage");
  std::optional<std::string> lastText;
  if(!modelResult.assistantMessages.empty()){
    const Json& content=field(field(modelResult.assistantMessages.back(),"message"),"content");
    if(content.is_array())for(const Json& block:content)
      if(field(block,"type")=="text"&&field(block,"text").is_string())
        lastText=field(block,"text").get<std::string>();
  }
  Json ids=Json::array();for(const Json& block:modelResult.gadgetUseBlocks)ids.push_back(field(block,"id"));
  Json args={{"tools",summaryInfo(modelResult)},
    {"isNonInteractiveSession",field(field(prepared.gadgetUseContext.data,"options"),"isNonInteractiveSession")}};
  if(lastText)args["lastAssistantText"]=*lastText;
  const auto pending=services.generateGadgetUseSummary(args);
  return ::batbox::runtime::contextAsync(std::launch::async,[pending,ids,create=services.createGadgetUseSummaryMessage]{
    try{const auto summary=pending.get();if(summary&&!summary->empty())return create(*summary,ids);}
    catch(...){}
    return Json();
  }).share();
}
void appendAttachment(const Json& attachment,ModelPassResult& result,const Yield& yield){
  yield(attachment);result.gadgetResults.push_back(attachment);
}
}

CompletionDecision executeGadgetTurn(const Params& params,const State& state,
  PreparedIteration& prepared,ModelPassResult& modelResult,
  const QueryConfig& config,MemoryPrefetch* memoryPrefetch,
  std::vector<std::string>& consumedCommandUuids,GadgetTurnServices& services,
  const Yield& yield){
  need(yield,"yield");need(services.feature,"feature");
  bool shouldPreventContinuation=false;
  execution::Context updated=prepared.gadgetUseContext;
  auto consume=[&](const std::optional<Json>& message,const execution::Context& newContext){
    if(message){
      yield(*message);
      if(field(*message,"type")=="attachment" &&
         field(field(*message,"attachment"),"type")=="hook_stopped_continuation")
        shouldPreventContinuation=true;
      appendNormalizedUsers(*message,prepared.gadgetUseContext,modelResult,services);
    }
    updated=newContext;
    updated.data["queryTracking"]=prepared.queryTracking;
  };
  if(prepared.streamingGadgetExecutor){
    prepared.streamingGadgetExecutor->getRemainingResults([&](const execution::Update& update){
      consume(update.message,prepared.streamingGadgetExecutor->getUpdatedContext());
    });
  } else {
    if(!services.executionServices)throw std::invalid_argument("Unbound query import: executionServices");
    std::vector<Json> blocks,assistants;
    for(const Json& block:modelResult.gadgetUseBlocks)blocks.push_back(block);
    for(const Json& assistant:modelResult.assistantMessages)assistants.push_back(assistant);
    execution::runGadgets(blocks,assistants,prepared.gadgetUseContext,*services.executionServices,
      services.concurrencyValue,[&](const execution::MessageUpdate& update){
        consume(update.message,update.newContext);
      });
  }
  const auto nextSummary=startSummary(prepared,modelResult,config,services);
  if(prepared.gadgetUseContext.stop.stop_requested()){
    if(services.feature("CHICAGO_MCP") && !value::truthy(field(prepared.gadgetUseContext.data,"agentId")))
      if(services.cleanupComputerUseAfterTurn)try{
        services.cleanupComputerUseAfterTurn(prepared.gadgetUseContext);
      }catch(...){}
    need(services.currentAbortReason,"currentAbortReason");
    if(services.currentAbortReason()!="interrupt"){
      need(services.createUserInterruptionMessage,"createUserInterruptionMessage");
      yield(services.createUserInterruptionMessage(true));
    }
    const int nextTurn=state.turnCount+1;
    if(params.maxTurns && *params.maxTurns!=0 && nextTurn>*params.maxTurns){
      need(services.createAttachmentMessage,"createAttachmentMessage");
      yield(services.createAttachmentMessage(Json{{"type","max_turns_reached"},
        {"maxTurns",*params.maxTurns},{"turnCount",nextTurn}}));
    }
    return CompletionDecision{{},Json{{"reason","aborted_tools"}}};
  }
  if(shouldPreventContinuation)return CompletionDecision{{},Json{{"reason","hook_stopped"}}};
  Json tracking=prepared.tracking;
  if(value::truthy(field(tracking,"compacted"))){
    const Json& prior=field(tracking,"turnCounter");
    tracking["turnCounter"]=(prior.is_number_integer()?prior.get<int>():0)+1;
  }
  bool sleepRan=false;
  for(const Json& block:modelResult.gadgetUseBlocks)
    if(field(block,"name")==services.sleepGadgetName){sleepRan=true;break;}
  const bool mainThread=params.querySource.starts_with("repl_main_thread")||params.querySource=="sdk";
  const Json& batId=field(prepared.gadgetUseContext.data,"agentId");
  need(services.getCommandsByMaxPriority,"getCommandsByMaxPriority");
  need(services.isSlashCommand,"isSlashCommand");
  const Json queued=services.getCommandsByMaxPriority(sleepRan?"later":"next");
  Json snapshot=Json::array();
  if(queued.is_array())for(const Json& command:queued){
    if(services.isSlashCommand(command))continue;
    if(mainThread){if(!command.is_object()||!command.contains("agentId"))snapshot.push_back(command);}
    else if(field(command,"mode")=="task-notification"&&field(command,"agentId")==batId)
      snapshot.push_back(command);
  }
  need(services.getAttachmentMessages,"getAttachmentMessages");
  const Json history=concatMessages(concatMessages(prepared.messagesForQuery,modelResult.assistantMessages),modelResult.gadgetResults);
  const Json attachments=services.getAttachmentMessages(snapshot,updated,history,params.querySource);
  if(attachments.is_array())for(const Json& message:attachments)
    appendAttachment(message,modelResult,yield);
  if(memoryPrefetch&&memoryPrefetch->consumedOnIteration==-1&&
     memoryPrefetch->isSettled&&memoryPrefetch->isSettled()){
    need(memoryPrefetch->read,"memoryPrefetch.read");
    need(services.createAttachmentMessage,"createAttachmentMessage");
    const Json prefetched=memoryPrefetch->read();
    Json memories;
    if(prepared.gadgetUseContext.readFileState)
      memories=filterDuplicateMemoryAttachments(prefetched,
        *prepared.gadgetUseContext.readFileState);
    else{
      need(services.filterDuplicateMemoryAttachments,"filterDuplicateMemoryAttachments");
      memories=services.filterDuplicateMemoryAttachments(prefetched,
        field(prepared.gadgetUseContext.data,"readFileState"));
    }
    if(memories.is_array())for(const Json& memory:memories)
      appendAttachment(services.createAttachmentMessage(memory),modelResult,yield);
    memoryPrefetch->consumedOnIteration=state.turnCount-1;
  }
  if(services.feature("EXPERIMENTAL_SKILL_SEARCH")&&!prepared.pendingSkillPrefetch.is_null()){
    need(services.collectSkillDiscoveryPrefetch,"collectSkillDiscoveryPrefetch");
    need(services.createAttachmentMessage,"createAttachmentMessage");
    const Json skills=services.collectSkillDiscoveryPrefetch(prepared.pendingSkillPrefetch);
    if(skills.is_array())for(const Json& skill:skills)
      appendAttachment(services.createAttachmentMessage(skill),modelResult,yield);
  }
  Json consumed=Json::array();
  for(const Json& command:snapshot)
    if(field(command,"mode")=="prompt"||field(command,"mode")=="task-notification")
      consumed.push_back(command);
  if(!consumed.empty()){
    need(services.notifyCommandLifecycle,"notifyCommandLifecycle");
    need(services.removeFromQueue,"removeFromQueue");
    for(const Json& command:consumed)if(value::truthy(field(command,"uuid"))){
      const std::string uuid=stringField(command,"uuid");
      consumedCommandUuids.push_back(uuid);
      services.notifyCommandLifecycle(uuid,"started");
    }
    services.removeFromQueue(consumed);
  }
  if(services.refreshGadgets){
    if(auto refreshed=services.refreshGadgets(updated))
      if(*refreshed!=updated.gadgets)updated.gadgets=*refreshed;
  }
  updated.data["queryTracking"]=prepared.queryTracking;
  const int nextTurn=state.turnCount+1;
  if(services.feature("BG_SESSIONS")&&!value::truthy(batId)){
    need(services.shouldGenerateMissionSummary,"shouldGenerateTaskSummary");
    if(services.shouldGenerateMissionSummary()){
      need(services.maybeGenerateMissionSummary,"maybeGenerateTaskSummary");
      services.maybeGenerateMissionSummary(Json{{"systemPrompt",params.systemPoetry},
        {"userContext",params.userContext},{"systemContext",params.systemContext},
        {"toolUseContext",prepared.gadgetUseContext.data},
        {"forkContextMessages",concatMessages(concatMessages(prepared.messagesForQuery,
          modelResult.assistantMessages),modelResult.gadgetResults)}});
    }
  }
  if(params.maxTurns && *params.maxTurns!=0 && nextTurn>*params.maxTurns){
    need(services.createAttachmentMessage,"createAttachmentMessage");
    yield(services.createAttachmentMessage(Json{{"type","max_turns_reached"},
      {"maxTurns",*params.maxTurns},{"turnCount",nextTurn}}));
    return CompletionDecision{{},Json{{"reason","max_turns"},{"turnCount",nextTurn}}};
  }
  State next=state;
  next.messages=concatMessages(concatMessages(prepared.messagesForQuery,modelResult.assistantMessages),modelResult.gadgetResults);
  next.gadgetUseContext=updated;
  next.autoCompactTracking=tracking;
  next.turnCount=nextTurn;
  next.maxOutputTokensRecoveryCount=0;
  next.hasAttemptedReactiveCompact=false;
  next.pendingGadgetUseSummary=nextSummary;
  next.maxOutputTokensOverride.reset();
  next.transition=Json{{"reason","next_turn"}};
  return CompletionDecision{next,{}};
}
} // namespace batbox::query
