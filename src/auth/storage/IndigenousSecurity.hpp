#pragma once
#include "auth/storage/MacKeychain.hpp"

namespace batbox::auth::storage {
MacKeychainOps indigenousMacKeychainOps(event_loop::Loop& loop,
  std::function<void(const std::string&,const std::string&)> debug,
  std::string executable="/usr/bin/security");
}
