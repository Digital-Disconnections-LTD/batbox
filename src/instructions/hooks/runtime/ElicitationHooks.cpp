#include "instructions/hooks/runtime/ElicitationHooks.hpp"
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
namespace {
void optionalField(HookDispatch& dispatch,const char* name,
  const std::optional<std::string>& value){
  if(value)dispatch.hookInput[name]=*value;
  else dispatch.ownUndefinedKeys.push_back(name);
}
void optionalField(HookDispatch& dispatch,const char* name,
  const std::optional<Json>& value){
  if(value)dispatch.hookInput[name]=*value;
  else dispatch.ownUndefinedKeys.push_back(name);
}
HookDispatch makeDispatch(const ElicitationHookServices& services,
  const std::string& event,const std::string& serverName,
  const std::optional<std::string>& permissionMode,
  std::optional<double> timeoutMs){
  if(!services.createBaseHookInput)
    throw std::invalid_argument("Elicitation base-input import required");
  const auto base=services.createBaseHookInput(permissionMode);
  HookDispatch dispatch;
  dispatch.hookInput=base.toJson();
  if(!base.permissionMode)dispatch.ownUndefinedKeys.push_back("permission_mode");
  if(!base.batId)dispatch.ownUndefinedKeys.push_back("agent_id");
  if(!base.batType)dispatch.ownUndefinedKeys.push_back("agent_type");
  dispatch.hookInput["hook_event_name"]=event;
  dispatch.hookInput["mcp_server_name"]=serverName;
  dispatch.matchQuery=serverName;
  dispatch.timeoutMs=timeoutMs.value_or(services.defaultTimeoutMs);
  return dispatch;
}
void execute(const ElicitationHookServices& services,
  const event_loop::Deferred<ElicitationHookOutput>& output,
  HookDispatch dispatch,const std::string& event,
  std::optional<std::stop_token> signal){
  if(!services.executeHooksOutsideRepl)
    throw std::invalid_argument("Elicitation executor import required");
  if(!services.schemaSafeParse)
    throw std::invalid_argument("Elicitation schema import required");
  services.executeHooksOutsideRepl(std::move(dispatch),signal).then(
    [output,event,parse=services.schemaSafeParse](
      const std::vector<HookResult>* results,const std::exception_ptr& failure){
      if(failure){output.reject(failure);return;}
      try{
        ElicitationHookOutput value;
        for(const auto& result:*results){
          const auto parsed=parseElicitationHookOutput(result,event,parse);
          if(parsed.blockingError)value.blockingError=parsed.blockingError;
          if(parsed.response)value.response=parsed.response;
        }
        output.resolve(std::move(value));
      }catch(...){output.reject(std::current_exception());}
    });
}
}
event_loop::Deferred<ElicitationHookOutput> executeElicitationHooks(
  event_loop::Loop& loop,const ElicitationHookServices& services,
  const ElicitationRequest& request){
  if(!loop.isOwnerThread())throw std::logic_error("Elicitation hook owner required");
  event_loop::Deferred<ElicitationHookOutput> output(loop);
  try{
    auto dispatch=makeDispatch(services,"Elicitation",request.serverName,
      request.permissionMode,request.timeoutMs);
    dispatch.hookInput["message"]=request.message;
    optionalField(dispatch,"mode",request.mode);
    optionalField(dispatch,"url",request.url);
    optionalField(dispatch,"elicitation_id",request.elicitationId);
    optionalField(dispatch,"requested_schema",request.requestedSchema);
    execute(services,output,std::move(dispatch),"Elicitation",request.signal);
  }catch(...){output.reject(std::current_exception());}
  return output;
}
event_loop::Deferred<ElicitationHookOutput> executeElicitationResultHooks(
  event_loop::Loop& loop,const ElicitationHookServices& services,
  const ElicitationResultRequest& request){
  if(!loop.isOwnerThread())throw std::logic_error("ElicitationResult hook owner required");
  event_loop::Deferred<ElicitationHookOutput> output(loop);
  try{
    auto dispatch=makeDispatch(services,"ElicitationResult",request.serverName,
      request.permissionMode,request.timeoutMs);
    optionalField(dispatch,"elicitation_id",request.elicitationId);
    optionalField(dispatch,"mode",request.mode);
    dispatch.hookInput["action"]=request.action;
    optionalField(dispatch,"content",request.content);
    execute(services,output,std::move(dispatch),"ElicitationResult",request.signal);
  }catch(...){output.reject(std::current_exception());}
  return output;
}
} // namespace batbox::instructions::hooks::runtime
