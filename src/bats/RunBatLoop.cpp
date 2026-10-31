#include "bats/RunBat.hpp"
#include <exception>
#include <stdexcept>

namespace batbox::bats {
namespace {
struct MaxTurnsReached {};
template<class T>void need(const T& callback,const char* name){
  if(!callback)throw std::invalid_argument(std::string("Unbound runAgent import: ")+name);
}
const Json& field(const Json& object,const char* name){return value::property(object,name);}
std::optional<std::string> uuid(const Json& message){
  const Json& value=field(message,"uuid");
  return value.is_string()?std::optional<std::string>(value.get<std::string>()):std::nullopt;
}
Json metadata(const RunBatParams& params){
  Json value={{"agentType",field(params.batDefinition,"agentType")}};
  if(params.worktreePath&&!params.worktreePath->empty())value["worktreePath"]=*params.worktreePath;
  if(params.description&&!params.description->empty())value["description"]=*params.description;
  return value;
}
void cleanup(const RunBatParams& params,const RunBatPreflight& preflight,
  RunBatPrepared& prepared,RunBatLoopServices& services){
  need(prepared.mcpCleanup,"mcpCleanup");
  prepared.mcpCleanup();
  if(value::truthy(field(params.batDefinition,"hooks"))){
    need(services.clearFlightHooks,"clearSessionHooks");
    services.clearFlightHooks(preflight.rootSetAppState,prepared.batId);
  }
  need(services.feature,"feature");
  if(services.feature("PROMPT_CACHE_BREAK_DETECTION")){
    need(services.cleanupBatTracking,"cleanupAgentTracking");
    services.cleanupBatTracking(prepared.batId);
  }
  if(prepared.context.query.readFileState)
    prepared.context.query.readFileState->clear();
  else{
    Json& fileState=prepared.context.query.data["readFileState"];
    if(services.clearReadFileStateStash)services.clearReadFileStateStash(fileState);
    else if(fileState.is_object()||fileState.is_array())fileState.clear();
  }
  prepared.initialMessages.clear();
  if(prepared.exposedStashSafeParams){
    prepared.exposedStashSafeParams->forkContextMessages.clear();
    Json& stashedContextMessages=
      prepared.exposedStashSafeParams->gadgetUseContext.query.data["messages"];
    if(stashedContextMessages.is_array())stashedContextMessages.clear();
  }
  Json& contextMessages=prepared.context.query.data["messages"];
  if(contextMessages.is_array())contextMessages.clear();
  need(services.clearBatTranscriptSubdir,"clearAgentTranscriptSubdir");
  services.clearBatTranscriptSubdir(prepared.batId);
  need(preflight.rootSetAppState,"rootSetAppState");
  preflight.rootSetAppState([batId=prepared.batId](const Json& previous){
    const Json& priorTodos=field(previous,"todos");
    if(!priorTodos.is_object()||!priorTodos.contains(batId))return previous;
    Json next=previous;
    Json todos=priorTodos;
    todos.erase(batId);
    next["todos"]=todos;
    return next;
  });
  need(services.killShellMissionsForBat,"killShellTasksForAgent");
  services.killShellMissionsForBat(prepared.batId,params.gadgetUseContext.getAppState,
    preflight.rootSetAppState);
  if(services.feature("MONITOR_TOOL")){
    need(services.killMonitorMcpMissionsForBat,"killMonitorMcpTasksForAgent");
    services.killMonitorMcpMissionsForBat(prepared.batId,
      params.gadgetUseContext.getAppState,preflight.rootSetAppState);
  }
}
}

void runBat(const RunBatParams& params,RunBatPreflightServices& preflightServices,
  RunBatSetupServices& setupServices,RunBatLoopServices& loopServices,
  const query::Yield& yield){
  const auto preflight=prepareRunBat(params,preflightServices);
  auto prepared=setupRunBat(params,preflight,setupServices);
  runPreparedBat(params,preflight,prepared,loopServices,yield);
}

void runPreparedBat(const RunBatParams& params,const RunBatPreflight& preflight,
  RunBatPrepared& prepared,RunBatLoopServices& loopServices,
  const query::Yield& yield){
  need(loopServices.enqueueInitialTranscript,"enqueueInitialTranscript");
  try{loopServices.enqueueInitialTranscript(prepared.initialMessages,prepared.batId);}catch(...){}
  need(loopServices.enqueueBatMetadata,"enqueueAgentMetadata");
  try{loopServices.enqueueBatMetadata(prepared.batId,metadata(params));}catch(...){}
  std::optional<std::string> lastRecordedUuid;
  if(!prepared.initialMessages.empty())lastRecordedUuid=uuid(prepared.initialMessages.back());
  std::exception_ptr prior;
  query::Params request;
  try{
    request.messages=prepared.initialMessages;
    request.systemPoetry=prepared.systemPoetry;
    request.userContext=prepared.userContext;
    request.systemContext=prepared.systemContext;
    request.gadgetUseContext=prepared.context.query;
    request.indigenousBatContext=std::make_shared<BatContext>(prepared.context);
    request.querySource=params.querySource;
    request.maxTurns=params.maxTurns;
    if(!request.maxTurns){
      const Json& maxTurns=field(params.batDefinition,"maxTurns");
      if(maxTurns.is_number_integer())request.maxTurns=maxTurns.get<int>();
    }
    const query::Yield consume=query::Yield::withReferences(
      [&](const query::MessageRef& messageRef){
      const Json& message=*messageRef;
      if(params.onQueryProgress)params.onQueryProgress();
      const Json& type=field(message,"type");
      if(type=="stream_event"&&field(field(message,"event"),"type")=="message_start"&&
         !field(message,"ttftMs").is_null()){
        if(params.gadgetUseContext.pushApiMetricsEntry)
          params.gadgetUseContext.pushApiMetricsEntry(field(message,"ttftMs"));
        return;
      }
      if(type=="attachment"){
        if(field(field(message,"attachment"),"type")=="max_turns_reached")
          throw MaxTurnsReached{};
        yield(messageRef);
        return;
      }
      if(isRecordableBatMessage(message)){
        need(loopServices.recordSidechainTranscript,"recordSidechainTranscript");
        try{loopServices.recordSidechainTranscript(Json::array({message}),
          prepared.batId,lastRecordedUuid);}catch(...){}
        if(type!="progress")lastRecordedUuid=uuid(message);
        yield(messageRef);
      }
    });
    try{
      if(loopServices.runIndigenousAsyncQuery)
        loopServices.runIndigenousAsyncQuery(request,prepared.context,
          params.canUseGadget,consume);
      else{
        need(loopServices.makeEngineForSubagent,"makeEngineForSubagent");
        auto engine=loopServices.makeEngineForSubagent(prepared.context,params.canUseGadget);
        if(loopServices.runQuery)(void)loopServices.runQuery(request,engine,consume);
        else (void)query::query(request,engine,consume);
      }
    }catch(const MaxTurnsReached&){}
    if(prepared.context.query.stop.stop_requested())throw BatAbortError{};
    need(loopServices.isBuiltInBat,"isBuiltInAgent");
    if(loopServices.isBuiltInBat(params.batDefinition)&&params.builtInCallback)
      params.builtInCallback();
  }catch(...){prior=std::current_exception();}
  cleanup(params,preflight,prepared,loopServices);
  request.messages.clear();
  if(prior)std::rethrow_exception(prior);
}
} // namespace batbox::bats
