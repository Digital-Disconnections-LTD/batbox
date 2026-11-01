#pragma once
#include "application/Config.hpp"
#include "app_state/State.hpp"
#include "user_input/UiCommandRouter.hpp"

namespace batbox::application_commands {
user_input::CommandEntry makeEffortCommand(event_loop::Loop& owner,
  std::shared_ptr<user_input::UiCommandRouter> router,
  const application::Config& config,
  std::shared_ptr<app_state::AppStateStore> store,
  std::function<bool()> supportsEffort);
}
