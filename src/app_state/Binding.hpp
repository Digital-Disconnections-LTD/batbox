#pragma once
#include "app_state/OnChange.hpp"
#include "flights/Lifecycle.hpp"

namespace batbox::app_state {
void bindFlightLifecycleNotifications(AppStateChangeServices& services,
  flights::Lifecycle& lifecycle);
AppStateStore makeAppStateStore(AppStateRef initial,
  AppStateChangeServices services);
} // namespace batbox::app_state
