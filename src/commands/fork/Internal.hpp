#pragma once
#include "commands/fork/ForkCommand.hpp"

namespace batbox::commands::fork::internal {
struct Prepared {
  Json command,batDefinition,poetryMessages;
  std::string args,batId,skillContent,commandName;
  bats::GetAppState modifiedGetAppState;
};
Prepared prepare(const Json& command,const std::string& args,Services& services);
bats::RunBatParams paramsFor(const Prepared& prepared,Services& services,
  bool async,const gadgets::Gadgets& availableGadgets);
event_loop::Deferred<user_input::InputResult> runForeground(
  Prepared prepared,Json preceding,Services& services);
event_loop::Deferred<user_input::InputResult> runBackground(
  Prepared prepared,Services& services);
void require(bool present,const char* name);
std::string errorMessage(const std::exception_ptr& error);
} // namespace batbox::commands::fork::internal
