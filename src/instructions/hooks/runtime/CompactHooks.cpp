#include "instructions/hooks/runtime/CompactHooks.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
namespace {
HookDispatch dispatchFor(const CompactHookServices& services,
  const std::string& event,const std::string& trigger,
  Json detail,std::optional<double> timeoutMs){
  if(!services.createBaseHookInput||!services.executeHooksOutsideRepl)
    throw std::invalid_argument("Compact hook imports required");
  const auto base=services.createBaseHookInput();
  HookDispatch dispatch;
  dispatch.hookInput=base.toJson();
  if(!base.permissionMode)dispatch.ownUndefinedKeys.push_back("permission_mode");
  if(!base.batId)dispatch.ownUndefinedKeys.push_back("agent_id");
  if(!base.batType)dispatch.ownUndefinedKeys.push_back("agent_type");
  dispatch.hookInput["hook_event_name"]=event;
  dispatch.hookInput["trigger"]=trigger;
  for(auto it=detail.begin();it!=detail.end();++it)
    dispatch.hookInput[it.key()]=it.value();
  dispatch.matchQuery=trigger;
  dispatch.timeoutMs=timeoutMs.value_or(services.defaultTimeoutMs);
  return dispatch;
}
std::string displayLine(const std::string& event,const HookResult& result){
  const auto output=value::trim(result.output);
  const auto label=event+" ["+result.command+"] ";
  if(result.succeeded)
    return label+"completed successfully"+
      (output.empty()?std::string{}:": "+output);
  return label+"failed"+(output.empty()?std::string{}:": "+output);
}
std::string join(const std::vector<std::string>& values,const std::string& separator){
  std::string output;bool first=true;
  for(const auto& value:values){
    if(!first)output+=separator;
    first=false;output+=value;
  }
  return output;
}
}
event_loop::Deferred<CompactHookOutput> executePreCompactHooks(
  event_loop::Loop& loop,const CompactHookServices& services,
  const std::string& trigger,const std::optional<std::string>& customInstructions,
  std::optional<std::stop_token> signal,std::optional<double> timeoutMs){
  if(!loop.isOwnerThread())throw std::logic_error("Compact hook owner required");
  event_loop::Deferred<CompactHookOutput> output(loop);
  try{
    auto dispatch=dispatchFor(services,"PreCompact",trigger,
      Json{{"custom_instructions",customInstructions?
        Json(*customInstructions):Json(nullptr)}},timeoutMs);
    services.executeHooksOutsideRepl(std::move(dispatch),signal).then(
      [output](const std::vector<HookResult>* results,
        const std::exception_ptr& error){
        if(error){output.reject(error);return;}
        try{
          CompactHookOutput value;
          if(results->empty()){output.resolve(std::move(value));return;}
          value.newCustomInstructionsOwnProperty=true;
          value.userDisplayMessageOwnProperty=true;
          std::vector<std::string> successfulOutputs,displayMessages;
          for(const auto& result:*results)
            if(result.succeeded&&!value::trim(result.output).empty())
              successfulOutputs.push_back(value::trim(result.output));
          for(const auto& result:*results)
            displayMessages.push_back(displayLine("PreCompact",result));
          if(!successfulOutputs.empty())
            value.newCustomInstructions=join(successfulOutputs,"\n\n");
          if(!displayMessages.empty())
            value.userDisplayMessage=join(displayMessages,"\n");
          output.resolve(std::move(value));
        }catch(...){output.reject(std::current_exception());}
      });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
event_loop::Deferred<CompactHookOutput> executePostCompactHooks(
  event_loop::Loop& loop,const CompactHookServices& services,
  const std::string& trigger,const std::string& compactSummary,
  std::optional<std::stop_token> signal,std::optional<double> timeoutMs){
  if(!loop.isOwnerThread())throw std::logic_error("Compact hook owner required");
  event_loop::Deferred<CompactHookOutput> output(loop);
  try{
    auto dispatch=dispatchFor(services,"PostCompact",trigger,
      Json{{"compact_summary",compactSummary}},timeoutMs);
    services.executeHooksOutsideRepl(std::move(dispatch),signal).then(
      [output](const std::vector<HookResult>* results,
        const std::exception_ptr& error){
        if(error){output.reject(error);return;}
        try{
          CompactHookOutput value;
          if(results->empty()){output.resolve(std::move(value));return;}
          value.userDisplayMessageOwnProperty=true;
          std::vector<std::string> displayMessages;
          for(const auto& result:*results)
            displayMessages.push_back(displayLine("PostCompact",result));
          if(!displayMessages.empty())
            value.userDisplayMessage=join(displayMessages,"\n");
          output.resolve(std::move(value));
        }catch(...){output.reject(std::current_exception());}
      });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
} // namespace batbox::instructions::hooks::runtime
