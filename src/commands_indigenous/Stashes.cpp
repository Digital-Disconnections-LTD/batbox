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
void deferred(event_loop::Loop& owner,const std::function<void()>& callback,
    const char* name){
  if(!callback)throw std::invalid_argument(
    std::string("Deferred clear Stash import required: ")+name);
  owner.queueMicrotask(callback);
}
}

void clearFlightStashes(StashServices& services,
    const std::vector<std::string>& preservedBatIds){
  const bool hasPreserved=!preservedBatIds.empty();
  call(services.clearUserContext,"clearUserContext");
  call(services.clearSystemContext,"clearSystemContext");
  call(services.clearGitStatus,"clearGitStatus");
  call(services.clearFlightStartDate,"clearSessionStartDate");
  call(services.clearFileSuggestions,"clearFileSuggestions");
  call(services.clearCommands,"clearCommands");
  if(!hasPreserved)call(services.resetPoetryStashBreakDetection,
    "resetPromptCacheBreakDetection");
  if(!services.setSystemPoetryInjection||!services.setLastEmittedDate)
    throw std::invalid_argument("Stash poetry/date setters required");
  services.setSystemPoetryInjection(nullptr);
  services.setLastEmittedDate(nullptr);
  call(services.runPostCompactCleanup,"runPostCompactCleanup");
  call(services.resetSentSkillNames,"resetSentSkillNames");
  if(!services.resetMemoryFilesStash)
    throw std::invalid_argument("Memory files Stash reset required");
  services.resetMemoryFilesStash("session_start");
  call(services.clearStoredImagePaths,"clearStoredImagePaths");
  call(services.clearAllFlights,"clearAllSessions");
  if(!hasPreserved)call(services.clearPendingPermissionCallbacks,
    "clearPendingPermissionCallbacks");
  if(!services.localUsageClearEnabled)
    throw std::invalid_argument("Local usage clear feature required");
  if(services.localUsageClearEnabled())
    deferred(services.loop,services.clearLocalUsage,"clearLocalUsage");
  if(!services.attributionEnabled)
    throw std::invalid_argument("Attribution clear feature required");
  if(services.attributionEnabled())
    deferred(services.loop,services.clearAttributionStashes,
      "clearAttributionCaches");
  call(services.clearRepositoryStashes,"clearRepositoryCaches");
  call(services.clearBashCommandPrefixes,"clearBashCommandPrefixes");
  if(!hasPreserved)call(services.clearDumpState,"clearDumpState");
  if(!services.clearInvokedSkills)
    throw std::invalid_argument("Invoked skills Stash required");
  services.clearInvokedSkills(preservedBatIds);
  call(services.clearGitDirResolution,"clearGitDirResolution");
  call(services.clearDynamicSkills,"clearDynamicSkills");
  call(services.resetLspDiagnostics,"resetLspDiagnostics");
  call(services.clearTrackedDocuments,"clearTrackedDocuments");
  call(services.clearFlightEnvironmentVariables,
    "clearSessionEnvironmentVariables");
  deferred(services.loop,services.clearWebFetchStash,"clearWebFetchCache");
  deferred(services.loop,services.clearGadgetSearchDescriptions,
    "clearToolSearchDescriptions");
  deferred(services.loop,services.clearBatDefinitions,
    "clearAgentDefinitions");
  deferred(services.loop,services.clearSkillPoetryStash,
    "clearSkillPromptCache");
}

void bindClearFlightStashes(ClearServices& clear,
    std::shared_ptr<StashServices> stashes){
  if(!stashes)throw std::invalid_argument("Stash services required");
  clear.clearFlightStashes=[stashes](const std::vector<std::string>& preserved){
    clearFlightStashes(*stashes,preserved);
  };
}
} // namespace batbox::commands_indigenous
