#pragma once
#include "bats/BatMissionNotification.hpp"

namespace batbox::bats {
// Bind LocalBatMission declarations to runAsyncBatLifecycle services.
// Remaining external services stay explicit.
void bindLocalBatMissionLifecycle(AsyncLifecycleImports& lifecycle,
  BatMissionStateServices& state,BatMissionNotificationServices& notification,
  BatMissionProgressImports& progress);
} // namespace batbox::bats
