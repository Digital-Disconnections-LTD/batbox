#include "application_hooks/Provider.hpp"
#include "instructions/hooks/runtime/HookRuntimeBindings.hpp"
#include <stdexcept>

namespace batbox::application_hooks {
event_loop::Deferred<std::string> Provider::createWorktreeHook(
    const std::string& name){
  if(!owner_.isOwnerThread())throw std::logic_error(
    "Worktree hook requires owner loop");
  if(!hasHooksFor("WorktreeCreate"))throw std::invalid_argument(
    "WorktreeCreate hook is not configured");
  auto services=hooks::bindWorktreeHookServices(owner_,{},baseServices_,
    executor_,inputs_.defaultTimeoutMs);
  // The application stores frontmatter/flight hooks in AppState. Keep that
  // state available for matching when the outside-REPL helper runs.
  services.executeHooksOutsideRepl=[this](
      instructions::hooks::HookDispatch dispatch){
    auto imports=executor_;
    imports.getAppState=inputs_.getAppState;
    return hooks::executeHooksOutsideRepl(owner_,dispatch,imports);
  };
  return hooks::executeWorktreeCreateHook(owner_,services,name);
}
} // namespace batbox::application_hooks
