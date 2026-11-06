#include "instructions/hooks/runtime/WorktreeHooks.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
namespace {
HookDispatch dispatchFor(const WorktreeHookServices& services,
  const std::string& event,const std::string& field,const std::string& value){
  if(!services.createBaseHookInput)
    throw std::invalid_argument("Worktree base-input import required");
  const auto base=services.createBaseHookInput();
  HookDispatch dispatch;
  dispatch.hookInput=base.toJson();
  if(!base.permissionMode)dispatch.ownUndefinedKeys.push_back("permission_mode");
  if(!base.batId)dispatch.ownUndefinedKeys.push_back("agent_id");
  if(!base.batType)dispatch.ownUndefinedKeys.push_back("agent_type");
  dispatch.hookInput["hook_event_name"]=event;
  dispatch.hookInput[field]=value;
  dispatch.timeoutMs=services.defaultTimeoutMs;
  return dispatch;
}
}
event_loop::Deferred<std::string> executeWorktreeCreateHook(
  event_loop::Loop& loop,const WorktreeHookServices& services,
  const std::string& name){
  if(!loop.isOwnerThread())throw std::logic_error("Worktree hook owner required");
  event_loop::Deferred<std::string> output(loop);
  try{
    auto dispatch=dispatchFor(services,"WorktreeCreate","name",name);
    if(!services.executeHooksOutsideRepl)
      throw std::invalid_argument("Worktree executor import required");
    services.executeHooksOutsideRepl(std::move(dispatch)).then(
      [output](const std::vector<HookResult>* results,
        const std::exception_ptr& failure){
        if(failure){output.reject(failure);return;}
        try{
          for(const auto& result:*results)
            if(result.succeeded&&!value::trim(result.output).empty()){
              output.resolve(value::trim(result.output));return;
            }
          std::string failedOutputs;
          for(const auto& result:*results)if(!result.succeeded){
            if(!failedOutputs.empty())failedOutputs+="; ";
            const auto trimmed=value::trim(result.output);
            failedOutputs+=result.command+": "+
              (trimmed.empty()?"no output":trimmed);
          }
          throw std::runtime_error("WorktreeCreate hook failed: "+
            (failedOutputs.empty()?"no successful output":failedOutputs));
        }catch(...){output.reject(std::current_exception());}
      });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
} // namespace batbox::instructions::hooks::runtime
