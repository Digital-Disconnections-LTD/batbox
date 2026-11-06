#include "query/GadgetTurn.hpp"
#include "query/MemoryAttachmentFilter.hpp"
#include <algorithm>
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
void appendAttachment(const Json& attachment,ModelPassResult& result,const Yield& yield){
  yield(attachment);result.gadgetResults.push_back(attachment);
}
}

CompletionDecision executeGadgetTurn(const Params& params,const State& state,
  PreparedIteration& prepared,ModelPassResult& modelResult,MemoryPrefetch* memoryPrefetch,
  std::vector<std::string>& consumedCommandUuids,GadgetTurnServices& services,
  const Yield& yield){
  need(yield,"yield");
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
  if(!services.executionServices)throw std::invalid_argument("Unbound query import: executionServices");
  std::vector<Json> blocks,assistants;
  for(const Json& block:modelResult.gadgetUseBlocks)blocks.push_back(block);
  for(const Json& assistant:modelResult.assistantMessages)assistants.push_back(assistant);
  execution::runGadgets(blocks,assistants,prepared.gadgetUseContext,*services.executionServices,
    services.concurrencyValue,[&](const execution::MessageUpdate& update){
      consume(update.message,update.newContext);
    });
  if(prepared.gadgetUseContext.stop.stop_requested()){
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
  next.maxOutputTokensOverride.reset();
  next.transition=Json{{"reason","next_turn"}};
  return CompletionDecision{next,{}};
}
} // namespace batbox::query
