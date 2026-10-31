#include "instructions/hooks/runtime/ConfigChangeHooks.hpp"
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
event_loop::Deferred<std::vector<HookResult>> executeConfigChangeHooks(
  event_loop::Loop& loop,const EnvHookServices& services,
  const std::string& source,std::optional<std::string> filePath,
  std::optional<double> timeoutMs){
  if(!loop.isOwnerThread())throw std::logic_error("Config hook owner required");
  if(!services.createBaseHookInput||!services.executeHooksOutsideRepl)
    throw std::invalid_argument("Config hook imports required");
  event_loop::Deferred<std::vector<HookResult>> output(loop);
  try{
    const auto base=services.createBaseHookInput();
    HookDispatch dispatch;
    dispatch.hookInput=base.toJson();
    if(!base.permissionMode)dispatch.ownUndefinedKeys.push_back("permission_mode");
    if(!base.batId)dispatch.ownUndefinedKeys.push_back("agent_id");
    if(!base.batType)dispatch.ownUndefinedKeys.push_back("agent_type");
    dispatch.hookInput["hook_event_name"]="ConfigChange";
    dispatch.hookInput["source"]=source;
    if(filePath)dispatch.hookInput["file_path"]=*filePath;
    else dispatch.ownUndefinedKeys.push_back("file_path");
    dispatch.timeoutMs=timeoutMs.value_or(services.defaultTimeoutMs);
    dispatch.matchQuery=source;
    services.executeHooksOutsideRepl(std::move(dispatch)).then(
      [output,source](const std::vector<HookResult>* values,
        const std::exception_ptr& error){
        if(error){output.reject(error);return;}
        try{
          auto results=*values;
          if(source=="policy_settings")
            for(auto& value:results)value.blocked=false;
          output.resolve(std::move(results));
        }catch(...){output.reject(std::current_exception());}
      });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
} // namespace batbox::instructions::hooks::runtime
