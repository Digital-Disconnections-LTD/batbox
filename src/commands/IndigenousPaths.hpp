#pragma once
#include "event_loop/Deferred.hpp"
#include <string>

namespace batbox::commands {
event_loop::Deferred<bool> indigenousPathExists(event_loop::Loop& loop,
  const std::string& path);
} // namespace batbox::commands
