#include "flights/Lifecycle.hpp"
namespace batbox::flights {
Lifecycle::Lifecycle(LifecycleServices services):services_(std::move(services)){}
void Lifecycle::setFlightStateChangedListener(StateListener cb){stateListener_=std::move(cb);}
void Lifecycle::setFlightMetadataChangedListener(MetadataListener cb){metadataListener_=std::move(cb);}
void Lifecycle::setPermissionModeChangedListener(PermissionListener cb){permissionListener_=std::move(cb);}
std::string Lifecycle::getFlightState()const{return currentState_;}
void Lifecycle::notifyFlightStateChanged(std::string state,value::ValueRef details){
  currentState_=state;
  // Copy callable identity so reentrant listener replacement cannot destroy the
  // active C++ std::function while its body is running.
  if(const auto listener=stateListener_)listener(state,details);
  if(state=="requires_action"&&details&&value::truthy(*details)){
    hasPendingAction_=true;
    if(const auto listener=metadataListener_){
      auto event=std::make_shared<value::SharedRecord>();event->setRef("pending_action",details);listener(std::move(event));
    }
  }else if(hasPendingAction_){
    hasPendingAction_=false;
    if(const auto listener=metadataListener_){
      auto event=std::make_shared<value::SharedRecord>();event->set("pending_action",nullptr);listener(std::move(event));
    }
  }
  if(state=="idle")if(const auto listener=metadataListener_){
    auto event=std::make_shared<value::SharedRecord>();event->set("task_summary",nullptr);listener(std::move(event));
  }
  if(services_.emitFlightStateEvents()){
    auto event=std::make_shared<value::SharedRecord>();event->set("type","system");event->set("subtype","session_state_changed");
    event->set("state",state);services_.enqueueSdkEvent(std::move(event));
  }
}
void Lifecycle::notifyFlightMetadataChanged(value::RecordRef metadata){
  if(const auto listener=metadataListener_)listener(std::move(metadata));
}
void Lifecycle::notifyPermissionModeChanged(const std::string& mode){
  if(const auto listener=permissionListener_)listener(mode);
}
} // namespace batbox::flights
