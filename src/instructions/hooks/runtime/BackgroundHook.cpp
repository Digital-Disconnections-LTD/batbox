#include "instructions/hooks/runtime/BackgroundHook.hpp"
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
bool executeInBackground(event_loop::Loop& loop,AsyncHookRoster& roster,
  BackgroundHookRequest request,const BackgroundHookServices& services){
  if(!loop.isOwnerThread())throw std::logic_error("Background hook owner required");
  if(!request.process||!request.process->resultCode||
    !request.process->getStdout||!request.process->getStderr||
    !request.process->cleanup||!request.background)
    throw std::invalid_argument("Live background hook process required");
  if(request.asyncRewake){
    if(!services.yieldToIo||!services.emitHookResponse||
      !services.wrapInSystemReminder||!services.enqueuePendingNotification||
      !services.onDetachedError)
      throw std::invalid_argument("Background hook notification services required");
    const auto process=request.process;
    request.process->resultCode().then(
      [request=std::move(request),services,process](const int* code,
        const std::exception_ptr& error){
        if(error){services.onDetachedError(error);return;}
        try{
          services.yieldToIo().then([request,services,process,code=*code](
            const event_loop::Unit*,const std::exception_ptr& error){
            if(error){services.onDetachedError(error);return;}
            try{
              process->getStdout().then([request,services,process,code](
                const std::string* stdoutText,const std::exception_ptr& error){
                if(error){services.onDetachedError(error);return;}
                try{
                  const auto stderrText=process->getStderr();
                  process->cleanup();
                  services.emitHookResponse({request.hookId,request.hookName,
                    request.hookEvent,*stdoutText+stderrText,*stdoutText,
                    stderrText,code==0?"success":"error",code});
                  if(code==2){
                    const auto message="Stop hook blocking error from command \""+
                      request.hookName+"\": "+
                      (stderrText.empty()?*stdoutText:stderrText);
                    services.enqueuePendingNotification(
                      services.wrapInSystemReminder(message),"task-notification");
                  }
                }catch(...){services.onDetachedError(std::current_exception());}
              });
            }catch(...){services.onDetachedError(std::current_exception());}
          });
        }catch(...){services.onDetachedError(std::current_exception());}
      });
    return true;
  }
  if(!request.background(request.processId))return false;
  AsyncHookRegistration registration;
  registration.processId=request.processId;
  registration.hookId=request.hookId;
  registration.hookName=request.hookName;
  registration.hookEvent=request.hookEvent;
  registration.command=request.command;
  registration.pluginId=request.pluginId;
  registration.asyncResponse=request.asyncResponse;
  registration.process=std::move(request.process);
  roster.registerPendingAsyncHook(registration);
  return true;
}
} // namespace batbox::instructions::hooks::runtime
