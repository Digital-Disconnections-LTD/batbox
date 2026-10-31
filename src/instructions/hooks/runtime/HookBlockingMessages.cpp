#include "instructions/hooks/runtime/HookBlockingMessages.hpp"

namespace batbox::instructions::hooks::runtime {
std::string getPreGadgetHookBlockingMessage(const std::string& hookName,
  const HookBlockingError& blockingError){
  return hookName+" hook error: "+blockingError.blockingError;
}
std::string getStopHookMessage(const HookBlockingError& blockingError){
  return "Stop hook feedback:\n"+blockingError.blockingError;
}
std::string getPartnerIdleHookMessage(const HookBlockingError& blockingError){
  return "TeammateIdle hook feedback:\n"+blockingError.blockingError;
}
std::string getMissionCreatedHookMessage(const HookBlockingError& blockingError){
  return "TaskCreated hook feedback:\n"+blockingError.blockingError;
}
std::string getMissionCompletedHookMessage(const HookBlockingError& blockingError){
  return "TaskCompleted hook feedback:\n"+blockingError.blockingError;
}
std::string getUserPoetrySubmitHookBlockingMessage(const HookBlockingError& blockingError){
  return "UserPromptSubmit operation blocked by hook:\n"+blockingError.blockingError;
}
} // namespace batbox::instructions::hooks::runtime
