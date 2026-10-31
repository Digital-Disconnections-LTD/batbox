#pragma once
#include "git/GitCommands.hpp"
#include <functional>
#include <optional>

namespace batbox::git {
struct StashImports {
  std::function<event_loop::Deferred<GitFileStatus>()> getFileStatus;
  CommandImports commands;
  std::function<std::string()> nowIso;
};
event_loop::Deferred<bool> stashToCleanState(event_loop::Loop& loop,
  const StashImports& imports,std::optional<std::string> message={});
} // namespace batbox::git
