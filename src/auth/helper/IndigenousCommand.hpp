#pragma once
#include "auth/helper/Helper.hpp"

namespace batbox::auth::helper {
event_loop::Deferred<CommandResult> indigenousExecuteShell(
  event_loop::Loop& loop,const std::string& command,double timeoutMs);
}
