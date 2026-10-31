#include "interactive/repl_runtime/Runtime.hpp"
#include <exception>
#include <stdexcept>

namespace batbox::interactive::repl_runtime {
namespace {
template<class T>void need(const T& value,const char* name){
  if(!value)throw std::invalid_argument(std::string("REPL runtime binding required: ")+name);
}
}
Runtime::Runtime(RuntimeServices services,TranscriptServices transcript,
    IdleServices idle):
  services_(std::move(services)),transcript_(std::move(transcript)),
  idle_(std::move(idle)),
  localCommands_(services_.initialLocalCommands){
  need(services_.readFileState,"readFileState");
  need(services_.onDetachedError,"onDetachedError");
}
Runtime::~Runtime(){
  try{stop();}
  catch(...){
    try{reportDetached(std::current_exception());}catch(...){}
  }
}
void Runtime::reportDetached(std::exception_ptr error)const{
  if(error)services_.onDetachedError(error);
}
void Runtime::start(){
  if(!services_.owner.isOwnerThread())
    throw std::logic_error("REPL runtime owner required");
  if(started_)return;
  stopPromise_.reset();
  auto weak=weak_from_this();
  if(weak.expired())throw std::logic_error("REPL runtime requires shared lifetime");
  need(services_.hasActiveLocalUi,"hasActiveLocalUi");
  need(services_.getSubmitCount,"getSubmitCount");
  need(services_.recordUserActivity,"recordUserActivity");
  need(services_.updateLastInteractionTime,"updateLastInteractionTime");
  need(services_.makeQueuedRequest,"makeQueuedRequest");
  need(services_.submitServices,"submitServices");
  need(services_.shutdownDiagnostics,"shutdownDiagnostics");
  need(transcript_.setHighlight,"setHighlight");
  need(transcript_.clearPositions,"clearPositions");
  need(transcript_.cancelEditorTimer,"cancelEditorTimer");
  started_=true;
  queueReadiness_=std::make_shared<submit::QueueReadiness>(
    services_.owner,services_.queue,services_.guard,
    services_.hasActiveLocalUi,
    [weak](submit::CommandList commands){
      if(auto self=weak.lock()){
        try{
          auto pending=self->executeQueuedInput(std::move(commands));
          pending.then([weak](const event_loop::Unit*,
              const std::exception_ptr& error){
            if(auto owner=weak.lock())owner->reportDetached(error);
          });
        }catch(...){self->reportDetached(std::current_exception());}
      }
    },
    [weak](std::exception_ptr error){
      if(auto self=weak.lock())self->reportDetached(error);
    });
  queueReadiness_->start();
  unsubscribeQueue_=services_.queue.subscribe([weak]{
    if(auto self=weak.lock())self->scheduleNowCheck();
  });
  scheduleNowCheck();
  observeInputActivity(services_.presentation.input.text,
    services_.getSubmitCount());
  observeTranscriptMode();
  try{
    auto init=onInit();
    init.then([weak](const event_loop::Unit*,const std::exception_ptr& error){
      if(auto self=weak.lock())self->reportDetached(error);
    });
  }catch(...){reportDetached(std::current_exception());}
}
void Runtime::stop(){
  if(!started_)return;
  (void)stopAndWait();
}
event_loop::Deferred<event_loop::Unit> Runtime::stopAndWait(){
  if(!services_.owner.isOwnerThread())
    throw std::logic_error("REPL shutdown requires owner loop");
  if(stopPromise_)return *stopPromise_;
  event_loop::Deferred<event_loop::Unit> completed(services_.owner);
  stopPromise_=completed;
  if(!started_){completed.resolve({});return completed;}
  started_=false;
  if(unsubscribeQueue_)unsubscribeQueue_();
  unsubscribeQueue_={};
  if(queueReadiness_)queueReadiness_->stop();
  queueReadiness_.reset();
  cancelIdleEffects();
  auto report=services_.onDetachedError;
  try{
    auto pending=services_.shutdownDiagnostics();
    if(!services_.owner.dispatcher().sameLoop(pending.dispatcher()))
      throw std::invalid_argument("Diagnostic shutdown Promise crossed owner loop");
    pending.then([report,completed](const event_loop::Unit*,
        const std::exception_ptr& error){
      if(error)try{report(error);}catch(...){}
      completed.resolve({});
    });
  }catch(...){
    try{report(std::current_exception());}catch(...){}
    completed.resolve({});
  }
  return completed;
}
void Runtime::scheduleNowCheck(){
  if(!started_||nowCheckScheduled_)return;
  nowCheckScheduled_=true;
  auto weak=weak_from_this();
  services_.owner.queueMicrotask([weak]{
    if(auto self=weak.lock()){
      self->nowCheckScheduled_=false;
      if(self->started_)self->observeQueuedCommands();
    }
  });
}
void Runtime::observeQueuedCommands(){
  for(const auto& command:services_.queue.get()){
    if(!command)continue;
    if(value::property(*command,"priority")!="now")continue;
    need(services_.getAbortController,"getAbortController");
    if(auto abort=services_.getAbortController())abort->abort("interrupt");
    return;
  }
}
void Runtime::observeInputActivity(const std::string& input,
    std::size_t submitCount){
  const auto previous=activitySnapshot_;
  if(previous&&previous->first==input&&previous->second==submitCount)return;
  activitySnapshot_=std::make_pair(input,submitCount);
  need(services_.recordUserActivity,"recordUserActivity");
  services_.recordUserActivity();
  need(services_.updateLastInteractionTime,"updateLastInteractionTime");
  services_.updateLastInteractionTime(true);
  if(submitCount==1&&(!previous||previous->second!=1)){
    need(services_.startBackgroundHousekeeping,"startBackgroundHousekeeping");
    services_.startBackgroundHousekeeping();
  }
}
} // namespace batbox::interactive::repl_runtime
