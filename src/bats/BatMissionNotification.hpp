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
} // namespace batbox::bats
