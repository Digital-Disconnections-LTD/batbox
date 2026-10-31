#include "commands_indigenous/RemainingCatalog.hpp"
#include <stdexcept>

namespace batbox::commands_indigenous {
std::vector<user_input::CommandEntry> buildRemainingCommands(
    const RemainingServices& services){
  if(!services.router||!services.rename||!services.cost||!services.exit||
     !services.rewind||!services.monster||!services.pages)
    throw std::invalid_argument("Remaining command services required");
  std::vector<user_input::CommandEntry> entries;
  entries.reserve(10);
  entries.push_back(makeRenameCommand(services.router,services.rename));
  entries.push_back(makePageCommand(PageKind::Status,services.router,services.pages));
  entries.push_back(makePageCommand(PageKind::Config,services.router,services.pages));
  entries.push_back(makePageCommand(PageKind::Bats,services.router,services.pages));
  entries.push_back(makePageCommand(PageKind::Missions,services.router,services.pages));
  entries.push_back(makePageCommand(PageKind::Permissions,services.router,services.pages));
  entries.push_back(makeCostCommand(services.cost));
  entries.push_back(makeRewindCommand(services.rewind));
  entries.push_back(makeExitCommand(services.router,services.exit));
  entries.push_back(makeMonsterCommand(services.router,services.monster));
  return entries;
}
std::vector<user_input::CommandEntry> buildIndigenousCommands(
    const CoreCommandServices& core,const RemainingServices& remaining){
  auto entries=buildCoreCommands(core);
  auto more=buildRemainingCommands(remaining);
  entries.reserve(entries.size()+more.size());
  for(auto& entry:more)entries.push_back(std::move(entry));
  return entries;
}
} // namespace batbox::commands_indigenous
