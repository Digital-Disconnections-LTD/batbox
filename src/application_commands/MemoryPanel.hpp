#pragma once
#include "application_memory/Provider.hpp"
#include "interactive/conversation_shell/Shell.hpp"
#include "user_input/UiCommandRouter.hpp"

namespace batbox::application_commands {
user_input::CommandEntry makeMemoryCommand(event_loop::Loop& owner,
  std::shared_ptr<user_input::UiCommandRouter> router,
  std::shared_ptr<application_memory::Provider> memory,
  std::function<std::shared_ptr<interactive::conversation_shell::Shell>()> shell);
}
