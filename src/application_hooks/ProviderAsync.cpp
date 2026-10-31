#include "application_hooks/Provider.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/Platform.hpp"
#include <stdexcept>

namespace batbox::application_hooks {
namespace {
std::string failureText(std::exception_ptr error){
  try{if(error)std::rethrow_exception(error);}
  catch(const std::exception& value){return value.what();}
  catch(...){return "unknown hook error";}
  return {};
}
bool sendsHookEvent(const std::string& event){
  return event=="SessionStart"||event=="Setup";
}
bool configuredAsync(const Json& hook){
  return value::truthy(value::property(hook,"async"))||
    value::truthy(value::property(hook,"asyncRewake"));
}
struct Progress:std::enable_shared_from_this<Progress>{
  event_loop::Loop& owner;
  std::function<event_loop::Deferred<Json>()> getOutput;
  std::function<void(const Json&)> emit;
  std::string id,name,event,last;
  event_loop::Loop::TimerHandle timer;
  bool stopped=false;
  Progress(event_loop::Loop& loop,
      std::function<event_loop::Deferred<Json>()> read,
      std::function<void(const Json&)> publish,std::string hookId,
      std::string hookName,std::string hookEvent):owner(loop),
      getOutput(std::move(read)),emit(std::move(publish)),id(std::move(hookId)),
      name(std::move(hookName)),event(std::move(hookEvent)){}
  void schedule(){
    if(stopped)return;
    const std::weak_ptr<Progress> weak=shared_from_this();
    timer=owner.setTimeout(1000,[weak]{if(auto self=weak.lock())self->poll();},false);
  }
  void poll(){
    if(stopped)return;
    try{
      getOutput().then([self=shared_from_this()](const Json* output,
          const std::exception_ptr& error){
        if(self->stopped)return;
        if(!error&&output){
          const auto& value=value::property(*output,"output");
          const auto text=value.is_string()?value.get<std::string>():std::string{};
          if(text!=self->last){
            self->last=text;
            if(self->emit)self->emit(Json{{"type","progress"},
              {"hookId",self->id},{"hookName",self->name},
              {"hookEvent",self->event},
              {"stdout",value::property(*output,"stdout")},
              {"stderr",value::property(*output,"stderr")},
              {"output",text}});
          }
        }
        self->schedule();
      });
    }catch(...){schedule();}
  }
  void stop(){stopped=true;timer.cancel();}
};
} // namespace

void Provider::bindCommandHooks(){
  hooks::AsyncHookServices rosterServices;
  rosterServices.nowMillis=command_.nowMillis;
  rosterServices.logDebug=command_.logDebug;
  rosterServices.startProgressInterval=[this](const std::string& id,
      const std::string& name,const std::string& event,
      std::function<event_loop::Deferred<Json>()> output){
    if(!sendsHookEvent(event)||!inputs_.emitHookEvent)
      return std::function<void()>([]{});
    auto progress=std::make_shared<Progress>(owner_,
      std::move(output),inputs_.emitHookEvent,id,name,event);
    progress->schedule();
    return std::function<void()>([progress]{progress->stop();});
  };
  rosterServices.emitHookResponse=[this](const hooks::AsyncHookResponseEvent& value){
    const auto content=!value.stdoutText.empty()?value.stdoutText:
      !value.stderrText.empty()?value.stderrText:value.output;
    if(!content.empty())inputs_.log("Hook "+value.hookName+" ("+
      value.hookEvent+") "+value.outcome+":\n"+content);
    if(sendsHookEvent(value.hookEvent)&&inputs_.emitHookEvent)
      inputs_.emitHookEvent(Json{{"type","response"},
        {"hookId",value.hookId},{"hookName",value.hookName},
        {"hookEvent",value.hookEvent},{"output",value.output},
        {"stdout",value.stdoutText},{"stderr",value.stderrText},
        {"exitCode",value.exitCode},{"outcome",value.outcome}});
  };
  rosterServices.invalidateFlightEnvStash=[environment=flightEnvironment_]{
    environment->invalidateFlightEnvStash();};
  rosterServices.stringifyThrown=failureText;
  asyncRoster_=std::make_shared<hooks::AsyncHookRoster>(owner_,rosterServices);
  hooks::FullCommandServices fullServices;
  fullServices.roster=asyncRoster_.get();
  fullServices.parseJson=[](const std::string& value){return Json::parse(value);};
  fullServices.stringifyThrown=failureText;
  fullServices.startProgressInterval=rosterServices.startProgressInterval;
  fullServices.currentAbortReason=inputs_.currentAbortReason?
    inputs_.currentAbortReason:[] {return std::string{};};
  fullServices.background.yieldToIo=[this]{
    event_loop::Deferred<event_loop::Unit> ready(owner_);
    owner_.queueNextTurn([ready]{ready.resolve({});});return ready;};
  fullServices.background.emitHookResponse=rosterServices.emitHookResponse;
  fullServices.background.wrapInSystemReminder=[](const std::string& text){
    return "<system-reminder>\n"+text+"\n</system-reminder>";};
  fullServices.background.enqueuePendingNotification=[this](
      const std::string& text,const std::string& mode){
    if(inputs_.enqueuePendingNotification)
      inputs_.enqueuePendingNotification(Json{{"value",text},{"mode",mode}});
    else inputs_.log("Hook notification: "+text);
  };
  fullServices.background.onDetachedError=[this](std::exception_ptr error){
    inputs_.log("Async hook failed: "+failureText(error));};
  const auto prepared=hooks::indigenousPreparedCommandHook(owner_,command_);
  const auto full=hooks::bindIndigenousFullCommandHook(owner_,command_,
    std::move(fullServices));
  executor_.execCommandHook=[this,prepared,full](hooks::CommandRequest request){
    if(sendsHookEvent(request.hookEvent)&&inputs_.emitHookEvent)
      inputs_.emitHookEvent(Json{{"type","started"},
        {"hookId",request.uuid},{"hookName",request.hookName},
        {"hookEvent",request.hookEvent}});
    return configuredAsync(request.hook)?full(std::move(request)):
      prepared(std::move(request));
  };
}

event_loop::Deferred<Json> Provider::asyncHookResponses(){
  if(!owner_.isOwnerThread())throw std::logic_error(
    "Async hook responses require owner loop");
  event_loop::Deferred<Json> output(owner_);
  if(!asyncRoster_){output.resolve(Json::array());return output;}
  asyncRoster_->checkForAsyncHookResponses().then(
    [roster=asyncRoster_,output](
        const std::vector<hooks::CheckedAsyncHookResponse>* responses,
        const std::exception_ptr& error){
      if(error){output.reject(error);return;}
      try{
        Json attachments=Json::array();
        std::vector<std::string> ids;
        for(const auto& response:*responses){
          ids.push_back(response.processId);
          Json attachment={{"type","async_hook_response"},
            {"processId",response.processId},{"hookName",response.hookName},
            {"hookEvent",response.hookEvent},{"response",response.response},
            {"stdout",response.stdoutText},{"stderr",response.stderrText},
            {"exitCode",response.exitCode}};
          if(response.gadgetName)attachment["toolName"]=*response.gadgetName;
          attachments.push_back(std::move(attachment));
        }
        roster->removeDeliveredAsyncHooks(ids);
        output.resolve(std::move(attachments));
      }catch(...){output.reject(std::current_exception());}
    });
  return output;
}

event_loop::Deferred<event_loop::Unit> Provider::finalizeAsyncHooks(){
  if(!owner_.isOwnerThread())throw std::logic_error(
    "Async hook shutdown requires owner loop");
  if(asyncRoster_)return asyncRoster_->finalizePendingAsyncHooks();
  event_loop::Deferred<event_loop::Unit> output(owner_);output.resolve({});
  return output;
}
bool Provider::hasActiveJobs()const{
  if(!owner_.isOwnerThread())throw std::logic_error("Hook activity requires owner loop");
  return (asyncRoster_&&asyncRoster_->size())||
    (poetryHost_&&poetryHost_->activeCount())||
    (batHost_&&batHost_->activeCount())||
    (functionHost_&&functionHost_->activeCount());
}
} // namespace batbox::application_hooks
