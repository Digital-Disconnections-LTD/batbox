#pragma once
#include "commands_indigenous/CoreCatalog.hpp"
#include "commands_indigenous/PageCommands.hpp"
#include "commands_indigenous/FlightCommands.hpp"

namespace batbox::commands_indigenous {
struct RemainingServices {
  std::shared_ptr<user_input::UiCommandRouter> router;
  std::shared_ptr<RenameServices> rename;
  std::shared_ptr<CostServices> cost;
  std::shared_ptr<ExitServices> exit;
  std::shared_ptr<RewindServices> rewind;
  std::shared_ptr<MonsterServices> monster;
  std::shared_ptr<PageServices> pages;
};

std::vector<user_input::CommandEntry> buildRemainingCommands(
  const RemainingServices& services);
std::vector<user_input::CommandEntry> buildIndigenousCommands(
  const CoreCommandServices& core,const RemainingServices& remaining);
} // namespace batbox::commands_indigenous
