#pragma once
#include "app_state/State.hpp"
#include "event_loop/Deferred.hpp"
#include "execution/GadgetExecution.hpp"
#include "instructions/hooks/InstructionsLoaded.hpp"
#include "instructions/project/async/AsyncDiscovery.hpp"
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace batbox::attachment_triggers {
using MemoryFiles=instructions::project::async::MemoryFiles;
using PathSet=instructions::project::PathSet;

struct Services {
  event_loop::Loop& loop;
  std::string cwd;
  instructions::project::async::Services memory;
  instructions::hooks::InstructionsLoadedImports hooks;
  std::function<app_state::AppStateRef()> getAppState;
  std::function<bool(const std::string&,const app_state::AppStateRef&)>
    pathInAllowedWorkingPath;
  std::function<bool()> skipProjectLevel;
  std::function<void(std::exception_ptr)> logError;
  std::function<double()> nowMillis;
};

struct DirectoriesToProcess {
  std::vector<std::string> nestedDirs,cwdLevelDirs;
};
DirectoriesToProcess getDirectoriesToProcess(const std::string& targetPath,
  const std::string& originalCwd,const std::string& cwd);
Json memoryFilesToAttachments(const MemoryFiles& files,execution::Context& context,
  const Services& services,const std::optional<std::string>& triggerFilePath={});
event_loop::Deferred<Json> getNestedMemoryAttachmentsForFile(
  const std::string& filePath,execution::Context& context,
  const app_state::AppStateRef& appState,const Services& services);
event_loop::Deferred<Json> getNestedMemoryAttachments(execution::Context& context,
  const Services& services);
event_loop::Deferred<Json> getDynamicSkillAttachments(execution::Context& context,
  const Services& services);
} // namespace batbox::attachment_triggers
