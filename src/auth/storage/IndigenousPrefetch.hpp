#pragma once
#include "auth/storage/KeychainPrefetch.hpp"

namespace batbox::auth::storage {
PrefetchServices indigenousPrefetchServices(event_loop::Loop& loop,
  std::shared_ptr<KeychainState> state,
  std::function<std::string()> platform,
  std::function<bool()> bareMode,
  std::string executable="/usr/bin/security");
}
