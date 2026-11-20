#pragma once
#include "application/memory/Provider.hpp"
#include "interactive/conversation_shell/Shell.hpp"
#include "user_input/UiCommandRouter.hpp"

namespace batbox::application::commands {
user_input::CommandEntry makeMemoryCommand(event_loop::Loop& owner,
  std::shared_ptr<user_input::UiCommandRouter> router,
  std::shared_ptr<application::memory::Provider> memory,
  std::function<std::shared_ptr<interactive::conversation_shell::Shell>()> shell);
}
