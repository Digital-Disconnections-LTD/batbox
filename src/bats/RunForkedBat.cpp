#include "bats/ForkedBat.hpp"
#include <stdexcept>

namespace batbox::bats {
namespace {
template<class T>void need(const T& callback,const char* name){
  if(!callback)throw std::invalid_argument(std::string("Unbound forked agent import: ")+name);
}
const Json& field(const Json& object,const char* name){return value::property(object,name);}
Json concatMessages(const Json& first,const Json& second){
  Json result=first.is_array()?first:Json::array();
  if(second.is_array())for(const Json& message:second)result.push_back(message);
  return result;
}
std::optional<std::string> uuidOf(const Json& message){
  const Json& uuid=field(message,"uuid");
  return uuid.is_string()?std::optional<std::string>(uuid.get<std::string>()):std::nullopt;
}
void clearFileState(BatContext& context,ContextServices& services){
  if(context.query.readFileState){context.query.readFileState->clear();return;}
  Json& fileState=context.query.data["readFileState"];
  if(services.clearFileStateStash)services.clearFileStateStash(fileState);
  else if(fileState.is_object()||fileState.is_array())fileState.clear();
}
}

ForkedBatResult runForkedBat(const ForkedBatParams& params,
  ForkServices& services){
  ForkedBatResult result;
  const StashSafeParams& stash=params.stashSafeParams;
  BatContext isolated=createSubagentContext(stash.gadgetUseContext,params.overrides,services.context);
  Json initialMessages=concatMessages(stash.forkContextMessages,params.poetryMessages);
  std::optional<std::string> transcriptBatId;
  if(!params.skipTranscript){
    need(services.context.createBatId,"createAgentId");
    transcriptBatId=services.context.createBatId(params.forkLabel);
  }
  std::optional<std::string> lastRecordedUuid;
  if(transcriptBatId){
    need(services.recordSidechainTranscript,"recordSidechainTranscript");
    try{services.recordSidechainTranscript(initialMessages,*transcriptBatId,std::nullopt);}catch(...){}
    if(initialMessages.is_array()&&!initialMessages.empty())
      lastRecordedUuid=uuidOf(initialMessages.back());
  }
  query::Params request;
  std::vector<std::shared_ptr<Json>> outputRefs;
  try{
    request.messages=initialMessages;
    request.systemPoetry=stash.systemPoetry;
    request.userContext=stash.userContext;
    request.systemContext=stash.systemContext;
    request.gadgetUseContext=isolated.query;
    request.querySource=params.querySource;
    request.maxOutputTokensOverride=params.maxOutputTokens;
    request.maxTurns=params.maxTurns;
    request.skipStashWrite=params.skipStashWrite;
    request.indigenousBatContext=std::make_shared<BatContext>(isolated);
    need(services.makeEngineForSubagent,"makeEngineForSubagent");
    auto engine=services.makeEngineForSubagent(isolated,params.canUseGadget);
    const query::Yield consume=query::Yield::withReferences(
      [&](const query::MessageRef& messageRef){
      const Json& message=*messageRef;
      const Json& type=field(message,"type");
      if(type=="stream_event"){
        const Json& event=field(message,"event");
        if(field(event,"type")=="message_delta"){
          const Json& usage=field(event,"usage");
          if(value::truthy(usage)){
            const Json turnUsage=updateUsage(EMPTY_USAGE,usage);
            result.totalUsage=accumulateUsage(result.totalUsage,turnUsage);
          }
        }
        return;
      }
      if(type=="stream_request_start")return;
      result.messages.push_back(message);
      outputRefs.push_back(messageRef);
      if(params.onMessageRef)params.onMessageRef(messageRef);
      else if(params.onMessage)params.onMessage(message);
      if(transcriptBatId&&(type=="assistant"||type=="user"||type=="progress")){
        need(services.recordSidechainTranscript,"recordSidechainTranscript");
        try{
          services.recordSidechainTranscript(Json::array({message}),*transcriptBatId,lastRecordedUuid);
        }catch(...){}
        if(type!="progress")lastRecordedUuid=uuidOf(message);
      }
    });
    if(services.runQuery)(void)services.runQuery(request,engine,consume);
    else (void)query::query(request,engine,consume);
  }catch(...){
    for(std::size_t i=0;i<outputRefs.size();++i)result.messages[i]=*outputRefs[i];
    clearFileState(isolated,services.context);
    initialMessages.clear();
    request.messages.clear();
    throw;
  }
  for(std::size_t i=0;i<outputRefs.size();++i)result.messages[i]=*outputRefs[i];
  clearFileState(isolated,services.context);
  initialMessages.clear();
  request.messages.clear();
  return result;
}
} // namespace batbox::bats
