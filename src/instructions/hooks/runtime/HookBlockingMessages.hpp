#pragma once
#include <string>

namespace batbox::instructions::hooks::runtime {
struct HookBlockingError {
  std::string blockingError,command;
};
std::string getPreGadgetHookBlockingMessage(const std::string& hookName,
  const HookBlockingError& blockingError);
std::string getStopHookMessage(const HookBlockingError& blockingError);
std::string getPartnerIdleHookMessage(const HookBlockingError& blockingError);
std::string getMissionCreatedHookMessage(const HookBlockingError& blockingError);
std::string getMissionCompletedHookMessage(const HookBlockingError& blockingError);
std::string getUserPoetrySubmitHookBlockingMessage(const HookBlockingError& blockingError);
} // namespace batbox::instructions::hooks::runtime
