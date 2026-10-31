#include "instructions/hooks/runtime/NotificationHooks.hpp"
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
event_loop::Deferred<event_loop::Unit> executeNotificationHooks(
  event_loop::Loop& loop,const EnvHookServices& services,
  const std::string& message,const std::optional<std::string>& title,
  const std::string& notificationType,std::optional<double> timeoutMs){
  if(!loop.isOwnerThread())throw std::logic_error("Notification hook owner required");
  if(!services.createBaseHookInput||!services.executeHooksOutsideRepl)
    throw std::invalid_argument("Notification hook imports required");
  event_loop::Deferred<event_loop::Unit> output(loop);
  try{
    const auto base=services.createBaseHookInput();
    HookDispatch dispatch;
    dispatch.hookInput=base.toJson();
    if(!base.permissionMode)dispatch.ownUndefinedKeys.push_back("permission_mode");
    if(!base.batId)dispatch.ownUndefinedKeys.push_back("agent_id");
    if(!base.batType)dispatch.ownUndefinedKeys.push_back("agent_type");
    dispatch.hookInput["hook_event_name"]="Notification";
    dispatch.hookInput["message"]=message;
    if(title)dispatch.hookInput["title"]=*title;
    else dispatch.ownUndefinedKeys.push_back("title");
    dispatch.hookInput["notification_type"]=notificationType;
    dispatch.timeoutMs=timeoutMs.value_or(services.defaultTimeoutMs);
    dispatch.matchQuery=notificationType;
    services.executeHooksOutsideRepl(std::move(dispatch)).then(
      [output](const std::vector<HookResult>*,const std::exception_ptr& error){
        if(error)output.reject(error);else output.resolve({});
      });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
} // namespace batbox::instructions::hooks::runtime
