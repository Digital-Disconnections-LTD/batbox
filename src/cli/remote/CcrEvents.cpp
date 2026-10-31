#include "cli/remote/CcrRuntime.hpp"
#include "runtime/JsonValue.hpp"

namespace batbox::cli::remote {
Deferred<Unit> CcrRuntime::flushStreamEventBuffer(){
  Deferred<Unit> answer(loop_);
  if(streamEventTimer_.valid())streamEventTimer_.cancel();
  streamEventTimer_={};
  if(streamEventBuffer_.empty()){
    answer.resolve({});return answer;
  }
  try{
    auto buffered=std::move(streamEventBuffer_);
    streamEventBuffer_.clear();
    const auto payloads=accumulateStreamEvents(buffered,
      streamTextAccumulator_);
    std::vector<Json> events;
    events.reserve(payloads.size());
    for(const auto& payload:payloads)
      events.push_back(Json{{"payload",payload},{"ephemeral",true}});
    auto queued=eventUploader_->enqueue(std::move(events));
    queued.then([answer](const Unit*,const std::exception_ptr& error){
      if(error)answer.reject(error);else answer.resolve({});
    });
  }catch(...){answer.reject(std::current_exception());}
  return answer;
}
Json CcrRuntime::toClientEvent(const Json& message)const{
  Json payload=message.is_object()?message:Json::object();
  if(!value::property(message,"uuid").is_string())
    payload["uuid"]=services_.randomUUID();
  return Json{{"payload",payload}};
}
Deferred<Unit> CcrRuntime::writeEvent(const Json& message){
  Deferred<Unit> answer(loop_);
  if(value::property(message,"type")=="stream_event"){
    streamEventBuffer_.push_back(message);
    if(!streamEventTimer_.valid()){
      const std::weak_ptr<CcrRuntime> weak=weak_from_this();
      streamEventTimer_=loop_.setTimeout(100,[weak]{
        if(const auto self=weak.lock())self->flushStreamEventBuffer();
      });
    }
    answer.resolve({});return answer;
  }
  auto pending=flushStreamEventBuffer();
  const std::weak_ptr<CcrRuntime> weak=weak_from_this();
  pending.then([weak,answer,message](const Unit*,
      const std::exception_ptr& error){
    if(error){answer.reject(error);return;}
    const auto self=weak.lock();if(!self){answer.resolve({});return;}
    try{
      if(value::property(message,"type")=="assistant")
        clearStreamAccumulatorForMessage(
          self->streamTextAccumulator_,message);
      auto queued=self->eventUploader_->enqueue(
        self->toClientEvent(message));
      queued.then([answer](const Unit*,const auto& failed){
        if(failed)answer.reject(failed);else answer.resolve({});
      });
    }catch(...){answer.reject(std::current_exception());}
  });
  return answer;
}
Deferred<Unit> CcrRuntime::writeInternalEvent(
    const std::string& eventType,const Json& payload,bool isCompaction,
    std::optional<std::string> batId){
  Deferred<Unit> answer(loop_);
  try{
    Json normalized={{"type",eventType}};
    if(payload.is_object())for(auto it=payload.begin();it!=payload.end();++it)
      normalized[it.key()]=it.value();
    if(!value::property(payload,"uuid").is_string())
      normalized["uuid"]=services_.randomUUID();
    Json event={{"payload",normalized}};
    if(isCompaction)event["is_compaction"]=true;
    if(batId&& !batId->empty())event["agent_id"]=*batId;
    auto queued=internalEventUploader_->enqueue(std::move(event));
    queued.then([answer](const Unit*,const std::exception_ptr& error){
      if(error)answer.reject(error);else answer.resolve({});
    });
  }catch(...){answer.reject(std::current_exception());}
  return answer;
}
Deferred<Unit> CcrRuntime::flushInternalEvents(){
  return internalEventUploader_->flush();
}
Deferred<Unit> CcrRuntime::flush(){
  Deferred<Unit> answer(loop_);
  auto pending=flushStreamEventBuffer();
  const std::weak_ptr<CcrRuntime> weak=weak_from_this();
  pending.then([weak,answer](const Unit*,const std::exception_ptr& error){
    if(error){answer.reject(error);return;}
    if(const auto self=weak.lock()){
      self->eventUploader_->flush().then([answer](const Unit*,
          const std::exception_ptr& failed){
        if(failed)answer.reject(failed);else answer.resolve({});
      });
    }else answer.resolve({});
  });
  return answer;
}
void CcrRuntime::reportDelivery(const std::string& eventId,
    const std::string& status){
  deliveryUploader_->enqueue(Json{{"eventId",eventId},{"status",status}});
}
}
