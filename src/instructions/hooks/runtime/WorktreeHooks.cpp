#include "instructions/hooks/runtime/WorktreeHooks.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
namespace {
std::optional<Json> configured(const std::optional<Json>& config,
  const std::string& event){
  if(!config||!config->is_object())return std::nullopt;
  const auto it=config->find(event);
  return it==config->end()?std::nullopt:std::optional<Json>(*it);
}
bool nonempty(const std::optional<Json>& value){
  return value&&value->is_array()&&!value->empty();
}
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
bool hasWorktreeCreateHook(const WorktreeHookServices& services){
  if(!services.getHooksConfigFromSnapshot)
    throw std::invalid_argument("Worktree snapshot import required");
  const auto snapshotHooks=configured(services.getHooksConfigFromSnapshot(),
    "WorktreeCreate");
  if(nonempty(snapshotHooks))return true;
  if(!services.getRegisteredHooks)
    throw std::invalid_argument("Worktree registry import required");
  const auto registeredHooks=configured(services.getRegisteredHooks(),
    "WorktreeCreate");
  if(!nonempty(registeredHooks))return false;
  if(!services.shouldAllowManagedHooksOnly)
    throw std::invalid_argument("Worktree managed policy import required");
  const bool managedOnly=services.shouldAllowManagedHooksOnly();
  for(const auto& matcher:*registeredHooks)
    if(!(managedOnly&&matcher.is_object()&&matcher.contains("pluginRoot")))
      return true;
  return false;
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
event_loop::Deferred<bool> executeWorktreeRemoveHook(
  event_loop::Loop& loop,const WorktreeHookServices& services,
  const std::string& worktreePath){
  if(!loop.isOwnerThread())throw std::logic_error("Worktree hook owner required");
  event_loop::Deferred<bool> output(loop);
  try{
    if(!services.getHooksConfigFromSnapshot)
      throw std::invalid_argument("Worktree snapshot import required");
    const auto snapshotHooks=configured(services.getHooksConfigFromSnapshot(),
      "WorktreeRemove");
    if(!services.getRegisteredHooks)
      throw std::invalid_argument("Worktree registry import required");
    const auto registeredHooks=configured(services.getRegisteredHooks(),
      "WorktreeRemove");
    if(!nonempty(snapshotHooks)&&!nonempty(registeredHooks)){
      output.resolve(false);return output;
    }
    auto dispatch=dispatchFor(services,"WorktreeRemove","worktree_path",
      worktreePath);
    if(!services.executeHooksOutsideRepl)
      throw std::invalid_argument("Worktree executor import required");
    services.executeHooksOutsideRepl(std::move(dispatch)).then(
      [output,log=services.logForDebugging](
        const std::vector<HookResult>* results,const std::exception_ptr& failure){
        if(failure){output.reject(failure);return;}
        try{
          if(results->empty()){output.resolve(false);return;}
          for(const auto& result:*results)if(!result.succeeded){
            if(!log)throw std::invalid_argument("Worktree debug import required");
            log("WorktreeRemove hook failed ["+result.command+"]: "+
              value::trim(result.output),"error");
          }
          output.resolve(true);
        }catch(...){output.reject(std::current_exception());}
      });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
} // namespace batbox::instructions::hooks::runtime
