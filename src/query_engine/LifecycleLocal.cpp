#include "query_engine/LifecycleInternal.hpp"

namespace batbox::query_engine::lifecycle {
bool Run::commandTag(const std::string& text)const{
  return text.find("<"+services->localCommandStdoutTag+">")!=std::string::npos||
    text.find("<"+services->localCommandStderrTag+">")!=std::string::npos;
}
void Run::localMessages(){
  while(localIndex<input.messages->size()){
    const auto& msg=*input.messages->at(localIndex);
    if(localPart==0){
      localPart=1;
      const auto& content=prop(prop(msg,"message"),"content");
      if(hasType(msg,"user")&&content.is_string()&&
          (commandTag(content.get<std::string>())||truth(prop(msg,"isCompactSummary")))){
        Json body=prop(msg,"message");body["content"]=services->stripAnsi(content.get<std::string>());
        Json sdk{{"type","user"},{"message",std::move(body)},
          {"session_id",services->getFlightId()},{"parent_tool_use_id",nullptr}};
        put(sdk,"uuid",prop(msg,"uuid"));put(sdk,"timestamp",prop(msg,"timestamp"));
        sdk["isReplay"]=!truth(prop(msg,"isCompactSummary"));
        put(sdk,"isSynthetic",truth(prop(msg,"isMeta"))?prop(msg,"isMeta"):prop(msg,"isVisibleInTranscriptOnly"));
        emit(std::move(sdk),Stage::LocalMessages);return;
      }
    }
    if(localPart==1){
      localPart=2;
      const auto& content=prop(msg,"content");
      if(hasType(msg,"system")&&prop(msg,"subtype")=="local_command"&&content.is_string()&&
          commandTag(content.get<std::string>())){
        emit(services->localCommandOutputToSDKAssistantMessage(content.get<std::string>(),prop(msg,"uuid")),
          Stage::LocalMessages);return;
      }
    }
    if(localPart==2){
      localPart=3;
      if(compact(msg)){
        Json sdk{{"type","system"},{"subtype","compact_boundary"},{"session_id",services->getFlightId()}};
        put(sdk,"uuid",prop(msg,"uuid"));
        put(sdk,"compact_metadata",services->toSDKCompactMetadata(prop(msg,"compactMetadata")));
        emit(std::move(sdk),Stage::LocalMessages);return;
      }
    }
    ++localIndex;localPart=0;
  }
  stage=Stage::LocalPersist;
}
void Run::localPersist(){
  if(persistFlight)await(services->recordTranscript(messages),[this](const event_loop::Unit&){
    flushThen([this]{stage=Stage::LocalResult;});
  });
  else stage=Stage::LocalResult;
}
void Run::localResult(){
  Json sdk=resultBase("success",false,static_cast<double>(messages->size())-1);
  sdk["result"]=input.resultText.value_or("");
  emit(std::move(sdk),Stage::Done);
}
void Run::flushThen(std::function<void()> next){
  if(persistFlight&&eagerFlush())await(services->flushFlightStorage(),
    [next=std::move(next)](const event_loop::Unit&){next();});
  else next();
}
}
