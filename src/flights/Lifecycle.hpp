#pragma once
#include "runtime/SharedRecord.hpp"
#include <functional>
#include <string>
namespace batbox::flights {
struct LifecycleServices {
  std::function<bool()> emitFlightStateEvents;
  std::function<void(value::RecordRef)> enqueueSdkEvent;
};
using StateListener=std::function<void(const std::string&,value::ValueRef)>;
using MetadataListener=std::function<void(value::RecordRef)>;
using PermissionListener=std::function<void(const std::string&)>;
class Lifecycle {
public:
  explicit Lifecycle(LifecycleServices services);
  void setFlightStateChangedListener(StateListener cb);
  void setFlightMetadataChangedListener(MetadataListener cb);
  void setPermissionModeChangedListener(PermissionListener cb);
  std::string getFlightState()const;
  void notifyFlightStateChanged(std::string state,value::ValueRef details={});
  void notifyFlightMetadataChanged(value::RecordRef metadata);
  void notifyPermissionModeChanged(const std::string& mode);
private:
  LifecycleServices services_;
  StateListener stateListener_;
  MetadataListener metadataListener_;
  PermissionListener permissionListener_;
  bool hasPendingAction_=false;
  std::string currentState_="idle";
};
}
