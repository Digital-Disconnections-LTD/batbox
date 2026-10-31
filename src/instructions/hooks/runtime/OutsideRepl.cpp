#include "instructions/hooks/runtime/OutsideReplInternal.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::instructions::hooks::runtime {
Json HookResult::toJson()const{
  Json value={{"command",command},{"succeeded",succeeded},
    {"output",output},{"blocked",blocked}};
  if(watchPaths)value["watchPaths"]=*watchPaths;
  if(systemMessage)value["systemMessage"]=*systemMessage;
  return value;
}
bool isAsyncHookJsonOutput(const Json& json){
  return json.is_object()&&json.contains("async")&&json.at("async")==true;
}
bool isSyncHookJsonOutput(const Json& json){return !isAsyncHookJsonOutput(json);}
std::string exceptionMessage(std::exception_ptr error){
  try{if(error)std::rethrow_exception(error);}
  catch(const std::exception& failure){return failure.what();}
  catch(...){return "unknown error";}
  return "";
}
std::string stringField(const Json& value,const char* key){
  return value.is_object()&&value.contains(key)&&value.at(key).is_string()
    ?value.at(key).get<std::string>():"";
}
std::optional<std::string> optionalString(const Json& value,const char* key){
  if(value.is_object()&&value.contains(key)&&value.at(key).is_string())
    return value.at(key).get<std::string>();
  return std::nullopt;
}
std::optional<double> timeoutSeconds(const Json& hook){
  if(hook.is_object()&&hook.contains("timeout")&&hook.at("timeout").is_number())
    return hook.at("timeout").get<double>();
  return std::nullopt;
}
bool syncBlocked(const Json& value){
  return isSyncHookJsonOutput(value)&&stringField(value,"decision")=="block";
}
std::optional<std::string> worktreePath(const Json& value){
  if(!isSyncHookJsonOutput(value)||!value.is_object()||
    !value.contains("hookSpecificOutput"))return std::nullopt;
  const auto& specific=value.at("hookSpecificOutput");
  if(stringField(specific,"hookEventName")!="WorktreeCreate")return std::nullopt;
  return optionalString(specific,"worktreePath");
}
OutsideRun::OutsideRun(event_loop::Loop& owner,HookDispatch value,
  ExecutorServices imports,std::stop_token signal):loop(owner),
  dispatch(std::move(value)),services(std::move(imports)),parentSignal(signal),
  output(owner){}
void OutsideRun::debug(const std::string& message,const std::string& level)const{
  if(!services.logDebug)throw std::invalid_argument("Hook debug logger required");
  services.logDebug(message,level);
}
void OutsideRun::fail(std::exception_ptr error){output.reject(error);}
void OutsideRun::start(){
  try{
    if(!services.simpleMode)throw std::invalid_argument("Hook simple-mode import required");
    if(services.simpleMode()){output.resolve({});return;}
    hookEvent=stringField(dispatch.hookInput,"hook_event_name");
    if(hookEvent.empty())throw std::invalid_argument("Hook event required");
    hookName=dispatch.matchQuery.empty()?hookEvent:hookEvent+":"+dispatch.matchQuery;
    if(!services.allHooksDisabled)
      throw std::invalid_argument("Hook managed-disable import required");
    if(services.allHooksDisabled()){
      debug("Skipping hooks for "+hookName+
        " due to 'disableAllHooks' managed setting");
      output.resolve({});return;
    }
    if(!services.skipDueToTrust)
      throw std::invalid_argument("Hook workspace-trust import required");
    if(services.skipDueToTrust()){
      debug("Skipping "+hookName+
        " hook execution - workspace trust not accepted");
      output.resolve({});return;
    }
    if(!services.getFlightId||(!services.getMatchingHooks&&
      !services.getMatchingHooksWithAppState))
      throw std::invalid_argument("Hook session and matching imports required");
    std::optional<Json> appState;
    if(services.getAppState)appState=services.getAppState();
    const auto flightId=services.getFlightId();
    auto pending=[&](){
      if(appState){
        if(!services.getMatchingHooksWithAppState)
          throw std::invalid_argument("Hook app-state matcher import required");
        return services.getMatchingHooksWithAppState(*appState,flightId,
          hookEvent,dispatch.hookInput);
      }
      if(!services.getMatchingHooks)
        throw std::invalid_argument("Hook matcher import required");
      return services.getMatchingHooks(flightId,hookEvent,dispatch.hookInput);
    }();
    pending.then(
      [self=shared_from_this()](const std::vector<MatchedHook>* hooks,
        const std::exception_ptr& error){
        if(error){self->fail(error);return;}
        try{self->afterMatching(*hooks);}catch(...){self->fail(std::current_exception());}
      });
  }catch(...){fail(std::current_exception());}
}
void OutsideRun::afterMatching(const std::vector<MatchedHook>& hooks){
  if(hooks.empty()||parentSignal.stop_requested()){output.resolve({});return;}
  try{jsonInput=value::stringify(dispatch.hookInput);}
  catch(...){
    if(!services.logError)throw;
    services.logError(exceptionMessage(std::current_exception()));
    output.resolve({});return;
  }
  results.resize(hooks.size());remaining=hooks.size();
  for(std::size_t index=0;index<hooks.size();index++){
    try{runOne(hooks[index],index);}
    catch(...){fail(std::current_exception());return;}
  }
}
void OutsideRun::finish(std::size_t index,HookResult result){
  results[index]=std::move(result);
  if(--remaining==0)output.resolve(std::move(results));
}
void OutsideRun::runOne(const MatchedHook& matched,std::size_t index){
  switch(matched.kind){
    case HookKind::Callback:runCallback(matched,index);return;
    case HookKind::Poetry:finish(index,{stringField(matched.hook,"prompt"),
      "Poetry stop hooks are not yet supported outside REPL",false,false});return;
    case HookKind::Bat:finish(index,{stringField(matched.hook,"prompt"),
      "Agent stop hooks are not yet supported outside REPL",false,false});return;
    case HookKind::Function:
      if(!services.logError)throw std::invalid_argument("Hook error logger required");
      services.logError("Function hook reached executeHooksOutsideREPL for "+
        hookEvent+". Function hooks should only be used in REPL context (Stop hooks).");
      finish(index,{"function",
        "Internal error: function hook executed outside REPL context",false,false});return;
    case HookKind::Http:runHttp(matched,index);return;
    case HookKind::Command:runCommand(matched,index);return;
  }
}
event_loop::Deferred<std::vector<HookResult>> executeHooksOutsideRepl(
  event_loop::Loop& loop,const HookDispatch& dispatch,
  const ExecutorServices& services,std::stop_token signal){
  if(!loop.isOwnerThread())throw std::logic_error("Hook executor owner required");
  auto run=std::make_shared<OutsideRun>(loop,dispatch,services,signal);
  run->start();return run->output;
}
std::function<event_loop::Deferred<Json>(HookDispatch)> bindOutsideReplExecutor(
  event_loop::Loop& loop,ExecutorServices services,std::stop_token signal){
  return [&loop,services=std::move(services),signal](HookDispatch dispatch){
    event_loop::Deferred<Json> output(loop);
    executeHooksOutsideRepl(loop,dispatch,services,signal).then(
      [output](const std::vector<HookResult>* rows,const std::exception_ptr& error){
        if(error){output.reject(error);return;}
        Json result=Json::array();
        for(const auto& row:*rows)result.push_back(row.toJson());
        output.resolve(std::move(result));
      });
    return output;
  };
}
} // namespace batbox::instructions::hooks::runtime
