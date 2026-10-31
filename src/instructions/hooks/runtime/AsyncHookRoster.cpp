#include "instructions/hooks/runtime/AsyncHookRoster.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
AsyncHookRoster::AsyncHookRoster(event_loop::Loop& loop,AsyncHookServices services):
  loop_(loop),services_(std::move(services)){
  if(!loop_.isOwnerThread())throw std::logic_error("Async hook registry owner required");
  if(!services_.nowMillis||!services_.logDebug||!services_.startProgressInterval||
    !services_.emitHookResponse||!services_.invalidateFlightEnvStash||
    !services_.stringifyThrown)
    throw std::invalid_argument("Async hook registry services required");
}
void AsyncHookRoster::log(const std::string& message,const std::string& level)const{
  services_.logDebug(message,level);
}
void AsyncHookRoster::erase(const std::string& processId){
  pending_.erase(processId);
  order_.erase(std::remove(order_.begin(),order_.end(),processId),order_.end());
}
std::size_t AsyncHookRoster::size()const{return pending_.size();}
void AsyncHookRoster::registerPendingAsyncHook(
  const AsyncHookRegistration& registration){
  if(!loop_.isOwnerThread())throw std::logic_error("Async hook registry owner required");
  if(!registration.process||!registration.process->getStdout||
    !registration.process->getStderr||!registration.process->resultCode||
    !registration.process->cleanup||!registration.process->kill||
    !registration.process->status)
    throw std::invalid_argument("Live async hook process required");
  const auto& asyncResponse=registration.asyncResponse;
  const double timeout=asyncResponse.is_object()&&
    asyncResponse.contains("asyncTimeout")&&
    value::truthy(asyncResponse.at("asyncTimeout"))
      ?asyncResponse.at("asyncTimeout").get<double>():15000;
  log("Hooks: Registering async hook "+registration.processId+" ("+
    registration.hookName+") with timeout "+
    value::numberToString(timeout)+"ms");
  const auto stop=services_.startProgressInterval(registration.hookId,
    registration.hookName,registration.hookEvent,
    [this,id=registration.processId](){
      event_loop::Deferred<Json> output(loop_);
      const auto found=pending_.find(id);
      if(found==pending_.end()||!found->second->process){
        output.resolve({{"stdout",""},{"stderr",""},{"output",""}});
        return output;
      }
      auto process=found->second->process;
      process->getStdout().then([output,process](const std::string* stdoutText,
        const std::exception_ptr& error){
        if(error){output.reject(error);return;}
        try{
          const auto stderrText=process->getStderr();
          output.resolve({{"stdout",*stdoutText},{"stderr",stderrText},
            {"output",*stdoutText+stderrText}});
        }catch(...){output.reject(std::current_exception());}
      });
      return output;
    });
  if(!pending_.contains(registration.processId))order_.push_back(registration.processId);
  pending_[registration.processId]=std::make_shared<PendingAsyncHook>(PendingAsyncHook{
    registration.processId,registration.hookId,registration.hookName,
    registration.hookEvent,registration.command,registration.gadgetName,
    registration.pluginId,services_.nowMillis(),timeout,false,
    registration.process,stop});
}
std::vector<PendingAsyncHook> AsyncHookRoster::getPendingAsyncHooks()const{
  if(!loop_.isOwnerThread())throw std::logic_error("Async hook registry owner required");
  std::vector<PendingAsyncHook> result;
  for(const auto& id:order_){
    const auto found=pending_.find(id);
    if(found!=pending_.end()&&!found->second->responseAttachmentSent)
      result.push_back(*found->second);
  }
  return result;
}
void AsyncHookRoster::removeDeliveredAsyncHooks(
  const std::vector<std::string>& processIds){
  if(!loop_.isOwnerThread())throw std::logic_error("Async hook registry owner required");
  for(const auto& id:processIds){
    const auto found=pending_.find(id);
    if(found!=pending_.end()&&found->second->responseAttachmentSent){
      log("Hooks: Removing delivered hook "+id);
      found->second->stopProgressInterval();erase(id);
    }
  }
}
void AsyncHookRoster::clearAllAsyncHooks(){
  if(!loop_.isOwnerThread())throw std::logic_error("Async hook registry owner required");
  for(const auto& [id,hook]:pending_)hook->stopProgressInterval();
  pending_.clear();order_.clear();
}
event_loop::Deferred<event_loop::Unit> AsyncHookRoster::finalizeHook(
  const PendingAsyncHook& hook,int exitCode,const std::string& outcome){
  event_loop::Deferred<event_loop::Unit> result(loop_);
  try{
    hook.stopProgressInterval();
    if(!hook.process){
      services_.emitHookResponse({hook.hookId,hook.hookName,hook.hookEvent,
        "","","",outcome,exitCode});
      result.resolve({});return result;
    }
    hook.process->getStdout().then([this,hook,result,exitCode,outcome](
      const std::string* stdoutText,const std::exception_ptr& error){
      if(error){result.reject(error);return;}
      try{
        const auto stderrText=hook.process->getStderr();
        hook.process->cleanup();
        services_.emitHookResponse({hook.hookId,hook.hookName,hook.hookEvent,
          *stdoutText+stderrText,*stdoutText,stderrText,outcome,exitCode});
        result.resolve({});
      }catch(...){result.reject(std::current_exception());}
    });
  }catch(...){result.reject(std::current_exception());}
  return result;
}
} // namespace batbox::instructions::hooks::runtime
