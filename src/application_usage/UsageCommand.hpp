#pragma once

#include "application/Config.hpp"
#include "application_model/LocalModelService.hpp"
#include "application_poetry/Provider.hpp"
#include "application_compaction_settings/Policy.hpp"
#include "app_state/State.hpp"
#include "interactive/PresentationState.hpp"
#include "user_input/UiCommandRouter.hpp"
#include <functional>
#include <memory>

namespace batbox::bats { struct BatContext; }
namespace batbox::interactive::conversation_shell { class Shell; }

namespace batbox::application_usage {

// All callbacks and reads occur on the application owner loop. The message
// vector holds shared references; analysis never copies the full transcript.
struct Sources {
  application::Config config;
  std::shared_ptr<app_state::AppStateStore> store;
  interactive::PresentationState* view=nullptr;
  std::shared_ptr<application_model::LocalModelService> model;
  std::shared_ptr<application_poetry::Provider> poetry;
  std::shared_ptr<application_compaction_settings::Policy> compactionPolicy;
  std::function<std::shared_ptr<bats::BatContext>()> getBatContext;
  std::function<std::shared_ptr<interactive::conversation_shell::Shell>()> getShell;
  std::function<std::string()> flightId;
  // Optional indigenous host lifetime counters. These are harness state, not
  // process RSS, model memory or GPU memory.
  std::function<Json()> runtimeMemory;
  bool interactive=true;
};

user_input::CommandEntry makeContextCommand(event_loop::Loop& owner,
  std::shared_ptr<user_input::UiCommandRouter> router,Sources sources);
user_input::CommandEntry makeStatusCommand(event_loop::Loop& owner,
  std::shared_ptr<user_input::UiCommandRouter> router,Sources sources);

} // namespace batbox::application_usage
