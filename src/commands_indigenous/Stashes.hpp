#pragma once
#include "event_loop/Loop.hpp"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace batbox::commands_indigenous {
struct ClearServices;
struct StashServices {
  event_loop::Loop& loop;
  explicit StashServices(event_loop::Loop& owner):loop(owner){}
  std::function<void()> clearUserContext,clearSystemContext,clearGitStatus,
    clearCommands,runPostCompactCleanup,clearPendingPermissionCallbacks;
  std::function<void(const std::string&)> resetMemoryFilesStash;
  std::function<void(const std::vector<std::string>&)> clearInvokedSkills;
};

void clearFlightStashes(StashServices& services,
  const std::vector<std::string>& preservedBatIds);
void bindClearFlightStashes(ClearServices& clear,
  std::shared_ptr<StashServices> stashes);
} // namespace batbox::commands_indigenous
