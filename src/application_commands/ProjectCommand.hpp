#pragma once
#include "user_input/SlashDispatch.hpp"
#include <filesystem>
#include <functional>

namespace batbox::application_commands {
// The optional stash refresh runs on the owner loop after init or reload I/O.
user_input::CommandEntry makeProjectCommand(event_loop::Loop& owner,
  std::filesystem::path cwd,std::function<void()> refresh={});
} // namespace batbox::application_commands
