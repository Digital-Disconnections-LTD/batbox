#pragma once
#include "application/Config.hpp"
#include "application_compaction_settings/Policy.hpp"
#include "application_model/LocalModelService.hpp"
#include "app_state/State.hpp"
#include "interactive/conversation_shell/Shell.hpp"
#include "user_input/UiCommandRouter.hpp"

namespace batbox::application_config {
user_input::CommandEntry makeConfigCommand(event_loop::Loop& owner,
  std::shared_ptr<user_input::UiCommandRouter> router,
  application::Config config,
  std::shared_ptr<application_model::LocalModelService> model,
  std::shared_ptr<app_state::AppStateStore> store,
  std::function<std::shared_ptr<interactive::conversation_shell::Shell>()> shell,
  std::shared_ptr<application_compaction_settings::Policy> policy={});
user_input::CommandEntry makeEndpointModelCommand(event_loop::Loop& owner,
  std::shared_ptr<user_input::UiCommandRouter> router,
  application::Config config,
  std::shared_ptr<application_model::LocalModelService> model,
  std::shared_ptr<app_state::AppStateStore> store,
  std::function<std::shared_ptr<interactive::conversation_shell::Shell>()> shell);
user_input::CommandEntry makeRoutedModelCommand(user_input::CommandEntry fallback,
  event_loop::Loop& owner,
  std::shared_ptr<user_input::UiCommandRouter> router,
  application::Config config,
  std::shared_ptr<application_model::LocalModelService> model,
  std::shared_ptr<app_state::AppStateStore> store,
  std::function<std::shared_ptr<interactive::conversation_shell::Shell>()> shell);
}
