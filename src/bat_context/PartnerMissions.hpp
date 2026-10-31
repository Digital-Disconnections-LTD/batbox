#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace batbox::bat_context {
struct PartnerMission {
  std::string type,status;
  Json isIdle;
  std::vector<std::function<void()>> onIdleCallbacks;
};
struct PartnerMissionState {
  std::vector<std::pair<std::string,std::shared_ptr<PartnerMission>>> missions;
};
using SetPartnerMissionState=std::function<void(
  std::function<PartnerMissionState(const PartnerMissionState&)>)>;
bool hasActiveInProcessPartners(const PartnerMissionState& state);
bool hasWorkingInProcessPartners(const PartnerMissionState& state);
event_loop::Deferred<event_loop::Unit> waitForPartnersToBecomeIdle(
  event_loop::Loop& loop,const SetPartnerMissionState& setAppState,
  const PartnerMissionState& appState);
}
