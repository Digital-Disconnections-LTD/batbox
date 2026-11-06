#include "instructions/hooks/runtime/HookBlockingMessages.hpp"

namespace batbox::instructions::hooks::runtime {
std::string getStopHookMessage(const HookBlockingError& blockingError){
  return "Stop hook feedback:\n"+blockingError.blockingError;
}
std::string getUserPoetrySubmitHookBlockingMessage(const HookBlockingError& blockingError){
  return "UserPromptSubmit operation blocked by hook:\n"+blockingError.blockingError;
}
} // namespace batbox::instructions::hooks::runtime
