#include "cli/remote/CcrRuntime.hpp"
#include "runtime/JsonValue.hpp"
#include <cmath>

namespace batbox::cli::remote {
namespace {
std::exception_ptr initError(const std::string& reason){
  return std::make_exception_ptr(CcrInitError(reason,
    "CCRClient init failed: "+reason));
}
}
Deferred<Json> CcrRuntime::initialize(std::optional<double> epoch){
  Deferred<Json> answer(loop_);
  Json auth;
  try{auth=services_.getAuthHeaders();}
  catch(...){answer.reject(std::current_exception());return answer;}
  if(!auth.is_object()||auth.empty()){
    answer.reject(initError("no_auth_headers"));return answer;
  }
  if(!epoch){
    const auto raw=services_.getEnvironmentWorkerEpoch();
    epoch=raw?value::parseInt10(*raw):std::numeric_limits<double>::quiet_NaN();
  }
  if(std::isnan(*epoch)){
    answer.reject(initError("missing_epoch"));return answer;
  }
  workerEpoch_=*epoch;
  auto restored=getWorkerState();
  Json body={{"worker_status","idle"},{"worker_epoch",workerEpoch_},
    {"external_metadata",{{"pending_action",nullptr},
      {"task_summary",nullptr}}}};
  auto registration=request("put","/worker",body,"PUT worker (init)");
  const std::weak_ptr<CcrRuntime> weak=weak_from_this();
  registration.then([weak,answer,restored](const CcrRequestResult* result,
      const std::exception_ptr& error){
    if(error||!result||!result->ok){
      answer.reject(initError("worker_register_failed"));return;
    }
    const auto self=weak.lock();
    if(!self){answer.reject(initError("worker_register_failed"));return;}
    self->currentState_="idle";
    self->startHeartbeat();
    self->services_.registerFlightActivity([weak]{
      if(const auto current=weak.lock())
        current->writeEvent(Json{{"type","keep_alive"}});
    });
    restored.then([answer](const Json* metadata,
        const std::exception_ptr& failure){
      if(failure)answer.reject(failure);
      else answer.resolve(metadata?*metadata:Json(nullptr));
    });
  });
  return answer;
}
void CcrRuntime::reportState(const Json& state,std::optional<Json> details){
  if(state==currentState_&&!details)return;
  currentState_=state;
  Json normalized=nullptr;
  if(details){
    normalized=Json::object();
    for(const auto* key:{"tool_name","action_description","request_id"})
      if(details->is_object()&&details->contains(key))
        normalized[key]=(*details)[key];
  }
  workerState_->enqueue(Json{{"worker_status",state},
    {"requires_action_details",normalized}});
}
void CcrRuntime::reportMetadata(const Json& metadata){
  workerState_->enqueue(Json{{"external_metadata",metadata}});
}
void CcrRuntime::stopHeartbeat(){
  if(heartbeatTimer_.valid())heartbeatTimer_.cancel();
  heartbeatTimer_={};heartbeatScheduled_=false;
}
void CcrRuntime::scheduleHeartbeat(){
  heartbeatScheduled_=true;
  const auto jitter=services_.heartbeatIntervalMs*
    services_.heartbeatJitterFraction*(2*services_.randomUnit()-1);
  const std::weak_ptr<CcrRuntime> weak=weak_from_this();
  heartbeatTimer_=loop_.setTimeout(
    services_.heartbeatIntervalMs+jitter,[weak]{
      if(const auto self=weak.lock()){
        self->sendHeartbeat();
        if(self->heartbeatScheduled_)self->scheduleHeartbeat();
      }
    });
}
void CcrRuntime::startHeartbeat(){
  stopHeartbeat();scheduleHeartbeat();
}
void CcrRuntime::sendHeartbeat(){
  if(heartbeatInFlight_)return;
  heartbeatInFlight_=true;
  auto pending=request("post","/worker/heartbeat",
    Json{{"session_id",flightId_},{"worker_epoch",workerEpoch_}},
    "Heartbeat",5000);
  const std::weak_ptr<CcrRuntime> weak=weak_from_this();
  pending.then([weak](const CcrRequestResult*,const std::exception_ptr&){
    if(const auto self=weak.lock())self->heartbeatInFlight_=false;
  });
}
std::size_t CcrRuntime::internalEventsPending()const{
  return internalEventUploader_->pendingCount();
}
void CcrRuntime::close(){
  closed_=true;stopHeartbeat();services_.unregisterFlightActivity();
  if(streamEventTimer_.valid())streamEventTimer_.cancel();
  streamEventTimer_={};streamEventBuffer_.clear();
  streamTextAccumulator_.byMessage.clear();
  streamTextAccumulator_.scopeToMessage.clear();
  workerState_->close();eventUploader_->close();
  internalEventUploader_->close();deliveryUploader_->close();
  transport_->setOnEvent({});
}
}
