#pragma once
#include "application/poetry/Provider.hpp"
#include "poetryEntries/SystemPoetryFile.hpp"
#include "user_input/SlashDispatch.hpp"

namespace batbox::application::commands {
// /poetry: where the system prompt head comes from and its first lines.
std::string poetryCommandText(const poetryEntries::SystemPoetrySource& source,
  bool simpleMode);
user_input::CommandEntry makePoetryCommand(event_loop::Loop& owner,
  std::shared_ptr<application::poetry::Provider> poetry);
} // namespace batbox::application::commands
