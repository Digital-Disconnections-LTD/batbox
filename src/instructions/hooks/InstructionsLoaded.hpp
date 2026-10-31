#pragma once
#include "event_loop/Deferred.hpp"
#include "instructions/project/ProjectDiscovery.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace batbox::instructions::hooks {
struct InstructionsLoadedOptions {
  std::optional<std::vector<std::string>> globs;
  std::optional<std::string> triggerFilePath,parentFilePath;
  std::optional<double> timeoutMs;
};
struct HookDispatch {
  Json hookInput;
  // JS object owns these fields with `undefined` values even though JSON
  // serialization omits them.
  std::vector<std::string> ownUndefinedKeys;
  double timeoutMs=0;
  std::string matchQuery;
};
struct InstructionsLoadedImports {
  std::function<std::optional<Json>()> getHooksConfigFromSnapshot;
  std::function<std::optional<Json>()> getRegisteredHooks;
  std::function<Json()> createBaseHookInput;
  std::function<event_loop::Deferred<Json>(HookDispatch)> executeHooksOutsideRepl;
  std::optional<double> defaultTimeoutMs;
};
bool hasInstructionsLoadedHook(const InstructionsLoadedImports& imports);
event_loop::Deferred<event_loop::Unit> executeInstructionsLoadedHooks(
  event_loop::Loop& loop,const InstructionsLoadedImports& imports,
  const std::string& filePath,const std::string& memoryType,
  const std::string& loadReason,const InstructionsLoadedOptions& options={});
void bindInstructionsLoaded(event_loop::Loop& loop,
  project::DiscoveryServices& discovery,InstructionsLoadedImports imports);
} // namespace batbox::instructions::hooks
