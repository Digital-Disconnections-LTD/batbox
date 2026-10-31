#pragma once
#include "git/GitCommands.hpp"
#include <optional>
#include <string>

namespace batbox::git {
event_loop::Deferred<std::optional<std::string>> findRemoteBase(
  event_loop::Loop& loop,CommandImports imports);
} // namespace batbox::git
