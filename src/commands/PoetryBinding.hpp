#pragma once
#include "bats/ForkedBat.hpp"
#include "skills/process/PoetryCommand.hpp"
#include "user_input/SlashDispatch.hpp"

namespace batbox::commands {
user_input::CommandEntry bindInlinePoetryCommand(event_loop::Loop& loop,
  Json descriptor,std::shared_ptr<bats::BatContext> context,
  std::shared_ptr<skills::process::PoetryCommandServices> services);
} // namespace batbox::commands
