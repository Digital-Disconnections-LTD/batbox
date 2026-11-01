#pragma once
#include "bats/BatMissionState.hpp"

namespace batbox::bats {
struct BatMissionNotificationServices {
  RootSetAppState setAppState;
  std::function<void(const RootSetAppState&)> abortSpeculation;
  std::function<std::string(const std::string&)> getMissionOutputPath;
  std::function<void(const Json&)> enqueuePendingNotification;
};
void enqueueBatNotification(const Json& args,
  BatMissionNotificationServices& services);
// The block sent when a Bat's queued input could not be continued: the leader
// learns which Bat holds undelivered input and why.
std::string batQueuedInputNotification(const std::string& missionId,
  const std::string& label,const std::string& reason);
} // namespace batbox::bats
