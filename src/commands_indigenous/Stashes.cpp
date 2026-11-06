#include "commands_indigenous/Stashes.hpp"
#include "commands_indigenous/Clear.hpp"
#include <stdexcept>

namespace batbox::commands_indigenous {
namespace {
template<class Callback>void call(const Callback& callback,const char* name){
  if(!callback)throw std::invalid_argument(
    std::string("Clear Stash import required: ")+name);
  callback();
}
}

void clearFlightStashes(StashServices& services,
    const std::vector<std::string>& preservedBatIds){
  const bool hasPreserved=!preservedBatIds.empty();
  call(services.clearUserContext,"clearUserContext");
  call(services.clearSystemContext,"clearSystemContext");
  call(services.clearGitStatus,"clearGitStatus");
  call(services.clearCommands,"clearCommands");
  call(services.runPostCompactCleanup,"runPostCompactCleanup");
  if(!services.resetMemoryFilesStash)
    throw std::invalid_argument("Memory files Stash reset required");
  services.resetMemoryFilesStash("session_start");
  if(!hasPreserved)call(services.clearPendingPermissionCallbacks,
    "clearPendingPermissionCallbacks");
  if(!services.clearInvokedSkills)
    throw std::invalid_argument("Invoked skills Stash required");
  services.clearInvokedSkills(preservedBatIds);
}

void bindClearFlightStashes(ClearServices& clear,
    std::shared_ptr<StashServices> stashes){
  if(!stashes)throw std::invalid_argument("Stash services required");
  clear.clearFlightStashes=[stashes](const std::vector<std::string>& preserved){
    clearFlightStashes(*stashes,preserved);
  };
}
} // namespace batbox::commands_indigenous
