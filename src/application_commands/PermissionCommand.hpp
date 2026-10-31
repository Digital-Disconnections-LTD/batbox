#pragma once
#include "application/Config.hpp"
#include "app_state/State.hpp"
#include "bootstrap/FlightTransitions.hpp"
#include "user_input/UiCommandRouter.hpp"

namespace batbox::application_commands {
user_input::CommandEntry makePermissionsCommand(event_loop::Loop& owner,
  std::shared_ptr<user_input::UiCommandRouter> router,
  const application::Config& config,
  std::shared_ptr<app_state::AppStateStore> store,
  std::shared_ptr<bootstrap::FlightTransitions> transitions);
}
