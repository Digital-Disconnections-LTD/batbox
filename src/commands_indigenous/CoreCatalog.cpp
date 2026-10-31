#include "commands_indigenous/CoreCatalog.hpp"
#include <stdexcept>

namespace batbox::commands_indigenous {
std::vector<user_input::CommandEntry> buildCoreCommands(
    const CoreCommandServices& services){
  if(!services.router||!services.clear||!services.compact||
     !services.model||!services.resume)
    throw std::invalid_argument("Core command services required");
  auto descriptions=std::make_shared<std::vector<Json>>();
  std::vector<user_input::CommandEntry> entries;
  entries.reserve(5);
  entries.push_back(makeHelpCommand(services.router,
    services.allCommands?services.allCommands:
      std::function<std::vector<Json>()>([descriptions]{return *descriptions;})));
  entries.push_back(makeClearCommand(services.clear));
  if(!services.compactEnabled||services.compactEnabled())
    entries.push_back(makeCompactCommand(services.compact));
  entries.push_back(makeModelCommand(services.router,services.model));
  entries.push_back(makeResumeCommand(services.router,services.resume));
  descriptions->reserve(entries.size());
  for(const auto& entry:entries)descriptions->push_back(entry.descriptor);
  return entries;
}
} // namespace batbox::commands_indigenous
