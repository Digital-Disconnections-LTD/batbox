#include "instructions/hooks/runtime/EnvHooks.hpp"
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
namespace {
HookDispatch makeDispatch(const EnvHookServices& services,double timeoutMs,
  Json fields){
  if(!services.createBaseHookInput)
    throw std::invalid_argument("Base hook input service required");
  const auto base=services.createBaseHookInput();
  HookDispatch dispatch;
  dispatch.hookInput=base.toJson();
  if(!base.permissionMode)dispatch.ownUndefinedKeys.push_back("permission_mode");
  if(!base.batId)dispatch.ownUndefinedKeys.push_back("agent_id");
  if(!base.batType)dispatch.ownUndefinedKeys.push_back("agent_type");
  for(auto it=fields.begin();it!=fields.end();++it)
    dispatch.hookInput[it.key()]=it.value();
  dispatch.timeoutMs=timeoutMs;
  return dispatch;
}
}
event_loop::Deferred<EnvHookResult> executeEnvHooks(event_loop::Loop& loop,
  const EnvHookServices& services,HookDispatch dispatch){
  if(!loop.isOwnerThread())throw std::logic_error("Env hook owner required");
  if(!services.executeHooksOutsideRepl||!services.invalidateFlightEnvStash)
    throw std::invalid_argument("Env hook execution services required");
  event_loop::Deferred<EnvHookResult> output(loop);
  try{
    services.executeHooksOutsideRepl(std::move(dispatch)).then(
      [output,invalidate=services.invalidateFlightEnvStash](
        const std::vector<HookResult>* results,const std::exception_ptr& error){
        if(error){output.reject(error);return;}
        try{
          if(!results->empty())invalidate();
          EnvHookResult value;value.results=*results;
          for(const auto& result:*results){
            if(result.watchPaths)
              for(const auto& path:*result.watchPaths)value.watchPaths.push_back(path);
          }
          for(const auto& result:*results)
            if(result.systemMessage&&!result.systemMessage->empty())
              value.systemMessages.push_back(*result.systemMessage);
          output.resolve(std::move(value));
        }catch(...){output.reject(std::current_exception());}
      });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
event_loop::Deferred<EnvHookResult> executeCwdChangedHooks(event_loop::Loop& loop,
  const EnvHookServices& services,const std::string& oldCwd,
  const std::string& newCwd,std::optional<double> timeoutMs){
  return executeEnvHooks(loop,services,makeDispatch(services,
    timeoutMs.value_or(services.defaultTimeoutMs),
    Json{{"hook_event_name","CwdChanged"},{"old_cwd",oldCwd},
      {"new_cwd",newCwd}}));
}
event_loop::Deferred<EnvHookResult> executeFileChangedHooks(event_loop::Loop& loop,
  const EnvHookServices& services,const std::string& filePath,
  const std::string& event,std::optional<double> timeoutMs){
  return executeEnvHooks(loop,services,makeDispatch(services,
    timeoutMs.value_or(services.defaultTimeoutMs),
    Json{{"hook_event_name","FileChanged"},{"file_path",filePath},
      {"event",event}}));
}
} // namespace batbox::instructions::hooks::runtime
