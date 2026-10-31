#include "query_engine/LifecycleInternal.hpp"

namespace batbox::query_engine::lifecycle {
void Run::startQuery(){
  QueryCall call;
  call.messages=messages;call.systemPoetry=systemPoetry;call.userContext=userContext;
  call.systemContext=parts.systemContext;call.canUseGadget=wrappedCanUseGadget;
  call.gadgetUseContext=context;call.fallbackModel=cfg.fallbackModel;
  call.maxTurns=cfg.maxTurns;call.missionBudget=cfg.missionBudget;
  queryCursor=services->query(call);queryActive=true;stage=Stage::QueryNext;
}
void Run::pullQuery(){
  inBody=false;
  await(queryCursor.next(),[this](const std::optional<MessageRef>& value){
    if(!value){queryActive=false;stage=Stage::FinishQuery;return;}
    message=*value;inBody=true;stage=Stage::QueryBeforeRecord;
  });
}
void Run::beforeRecord(){
  if(hasType(*message,"assistant")||hasType(*message,"user")||compact(*message)){
    if(persistFlight&&compact(*message)){
      const auto& tail=prop(prop(prop(*message,"compactMetadata"),"preservedSegment"),"tailUuid");
      if(truth(tail)){
        for(auto index=state->mutableMessages->size();index>0;--index){
          if(prop(*state->mutableMessages->at(index-1),"uuid")==tail){
            auto slice=std::make_shared<Messages>(state->mutableMessages->begin(),
              state->mutableMessages->begin()+static_cast<std::ptrdiff_t>(index));
            await(services->recordTranscript(slice),[this](const event_loop::Unit&){record();});return;
          }
        }
      }
    }
    record();
  }else stage=Stage::Dispatch;
}
void Run::record(){
  messages->push_back(message);
  if(persistFlight){
    auto pending=services->recordTranscript(messages);
    if(!hasType(*message,"assistant")){
      await(std::move(pending),[this](const event_loop::Unit&){stage=Stage::QueryAfterRecord;});return;
    }
  }
  stage=Stage::QueryAfterRecord;
}
void Run::acknowledge(){
  if(stage==Stage::QueryAfterRecord){
    if(hasAcknowledgedInitialMessages||messagesToAck.empty()){stage=Stage::Dispatch;return;}
    hasAcknowledgedInitialMessages=true;ackIndex=0;
  }
  while(ackIndex<messagesToAck.size()){
    const auto msg=messagesToAck[ackIndex++];
    if(!hasType(*msg,"user"))continue;
    Json sdk{{"type","user"}};put(sdk,"message",prop(*msg,"message"));
    sdk["session_id"]=services->getFlightId();sdk["parent_tool_use_id"]=nullptr;
    put(sdk,"uuid",prop(*msg,"uuid"));put(sdk,"timestamp",prop(*msg,"timestamp"));sdk["isReplay"]=true;
    emit(std::move(sdk),Stage::Ack);return;
  }
  stage=Stage::Dispatch;
}
void Run::dispatch(){
  const auto type=prop(*message,"type");
  if(type=="user")++turnCount;
  if(type=="assistant"||type=="progress"||type=="user"){
    if(type=="assistant"){
      const auto& reason=prop(prop(*message,"message"),"stop_reason");
      if(!nullish(reason))lastStopReason=reason;
    }
    state->mutableMessages->push_back(message);
    if(type=="progress"&&persistFlight){messages->push_back(message);(void)services->recordTranscript(messages);}
    normalized=services->normalizeMessage(message);stage=Stage::NormalizedNext;return;
  }
  if(type=="stream_event"){
    const auto& event=prop(*message,"event");const auto& eventType=prop(event,"type");
    if(eventType=="message_start"){
      currentMessageUsage=EMPTY_USAGE;
      currentMessageUsage=updateUsage(currentMessageUsage,prop(prop(event,"message"),"usage"));
    }
    if(eventType=="message_delta"){
      currentMessageUsage=updateUsage(currentMessageUsage,prop(event,"usage"));
      const auto& reason=prop(prop(event,"delta"),"stop_reason");
      if(!nullish(reason))lastStopReason=reason;
    }
    if(eventType=="message_stop")state->totalUsage=std::make_shared<Json>(
      accumulateUsage(*state->totalUsage,currentMessageUsage));
    if(cfg.includePartialMessages){
      Json sdk{{"type","stream_event"},{"event",event},{"session_id",services->getFlightId()},
        {"parent_tool_use_id",nullptr},{"uuid",services->randomUUID()}};
      emit(std::move(sdk),Stage::PostSwitch);return;
    }
  }else if(type=="attachment"){attachment();return;}
  else if(type=="system"){systemMessage();return;}
  else if(type=="tool_use_summary"){
    Json sdk{{"type","tool_use_summary"}};put(sdk,"summary",prop(*message,"summary"));
    put(sdk,"preceding_tool_use_ids",prop(*message,"precedingToolUseIds"));
    sdk["session_id"]=services->getFlightId();put(sdk,"uuid",prop(*message,"uuid"));
    emit(std::move(sdk),Stage::PostSwitch);return;
  }
  stage=Stage::PostSwitch;
}
void Run::normalizedNext(){
  if(auto value=normalized.next())emit(std::move(*value),Stage::NormalizedNext);
  else {normalized={};stage=Stage::PostSwitch;}
}
void Run::attachment(){
  state->mutableMessages->push_back(message);
  if(persistFlight){messages->push_back(message);(void)services->recordTranscript(messages);}
  const auto& data=prop(*message,"attachment");const auto& type=prop(data,"type");
  if(type=="structured_output")structuredOutput=prop(data,"data");
  else if(type=="max_turns_reached"){
    flushThen([this]{
      const auto& data=prop(*message,"attachment");
      limitResult("error_max_turns",value::numberOr(prop(data,"turnCount")),
        "Reached maximum number of turns ("+string(prop(data,"maxTurns"))+")");
    });return;
  }else if(cfg.replayUserMessages&&type=="queued_command"){
    Json body{{"role","user"}};put(body,"content",prop(data,"prompt"));
    Json sdk{{"type","user"},{"message",std::move(body)},
      {"session_id",services->getFlightId()},{"parent_tool_use_id",nullptr}};
    put(sdk,"uuid",truth(prop(data,"source_uuid"))?prop(data,"source_uuid"):prop(*message,"uuid"));
    put(sdk,"timestamp",prop(*message,"timestamp"));sdk["isReplay"]=true;
    emit(std::move(sdk),Stage::PostSwitch);return;
  }
  stage=Stage::PostSwitch;
}
void Run::systemMessage(){
  if(state->config->snipReplay){
    const auto replay=state->config->snipReplay(message,state->mutableMessages);
    if(replay){
      if(replay->executed){
        state->mutableMessages->clear();
        state->mutableMessages->insert(state->mutableMessages->end(),replay->messages->begin(),replay->messages->end());
      }
      stage=Stage::PostSwitch;return;
    }
  }
  state->mutableMessages->push_back(message);
  if(compact(*message)&&truth(prop(*message,"compactMetadata"))){
    if(state->mutableMessages->size()>1)state->mutableMessages->erase(
      state->mutableMessages->begin(),state->mutableMessages->end()-1);
    if(messages->size()>1)messages->erase(messages->begin(),messages->end()-1);
    Json sdk{{"type","system"},{"subtype","compact_boundary"},{"session_id",services->getFlightId()}};
    put(sdk,"uuid",prop(*message,"uuid"));
    put(sdk,"compact_metadata",services->toSDKCompactMetadata(prop(*message,"compactMetadata")));
    emit(std::move(sdk),Stage::SystemAfterCompact);return;
  }
  systemAfterCompact();
}
void Run::systemAfterCompact(){
  if(prop(*message,"subtype")=="api_error"){
    Json sdk{{"type","system"},{"subtype","api_retry"}};
    put(sdk,"attempt",prop(*message,"retryAttempt"));put(sdk,"max_retries",prop(*message,"maxRetries"));
    put(sdk,"retry_delay_ms",prop(*message,"retryInMs"));
    const auto& error=prop(*message,"error");const auto& status=prop(error,"status");
    sdk["error_status"]=nullish(status)?Json(nullptr):status;
    put(sdk,"error",services->categorizeRetryableAPIError(error));
    sdk["session_id"]=services->getFlightId();put(sdk,"uuid",prop(*message,"uuid"));
    emit(std::move(sdk),Stage::PostSwitch);return;
  }
  stage=Stage::PostSwitch;
}
}
