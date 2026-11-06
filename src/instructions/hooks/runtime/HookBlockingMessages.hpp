#pragma once
#include <string>

namespace batbox::instructions::hooks::runtime {
struct HookBlockingError {
  std::string blockingError,command;
};
std::string getStopHookMessage(const HookBlockingError& blockingError);
std::string getUserPoetrySubmitHookBlockingMessage(const HookBlockingError& blockingError);
} // namespace batbox::instructions::hooks::runtime
