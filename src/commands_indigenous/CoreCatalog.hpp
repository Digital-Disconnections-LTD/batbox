#pragma once
#include "commands_indigenous/Clear.hpp"
#include "commands_indigenous/Compact.hpp"
#include "commands_indigenous/UiCommands.hpp"

namespace batbox::commands_indigenous {
struct CoreCommandServices {
  std::shared_ptr<user_input::UiCommandRouter> router;
  std::shared_ptr<ClearServices> clear;
  std::shared_ptr<CompactServices> compact;
  std::function<bool()> compactEnabled;
  std::shared_ptr<ModelServices> model;
  std::shared_ptr<ResumeServices> resume;
  std::function<std::vector<Json>()> allCommands;
};

// Construct on the owner loop when the command list is refreshed. Dynamic
// model metadata is evaluated at registration time.
std::vector<user_input::CommandEntry> buildCoreCommands(
  const CoreCommandServices& services);
} // namespace batbox::commands_indigenous
