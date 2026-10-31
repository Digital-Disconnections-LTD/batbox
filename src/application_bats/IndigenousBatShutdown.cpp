#include "application_bats/IndigenousBatApplication.hpp"
#include "application/Async.hpp"
#include <stdexcept>

namespace batbox::application_bats {
bool IndigenousBatApplication::backgroundForegroundMission(const std::string& missionId){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("Agent background action requires owner");
  if(closing_)return false;
  return bats::backgroundBatMission(missionId,foreground_);
}
void IndigenousBatApplication::stopMission(const std::string& missionId){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("Agent stop action requires owner");
  bats::killAsyncBat(missionId,missionState_);
}
event_loop::Deferred<event_loop::Unit> IndigenousBatApplication::shutdown(){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("Agent shutdown requires owner");
  if(shutdownResult_)return *shutdownResult_;
  shutdownResult_.emplace(inputs_.owner);closing_=true;capacityChanged_.notify_all();
  conversationChanged_.notify_all();
  // Invoke mission cleanup registrations outside the
  // roster lock: mission completion unregisters its own cleanup callback.
  std::vector<std::function<void()>> pending;
  {std::lock_guard lock(cleanupMutex_);
    for(const auto& [_,callback]:cleanups_)pending.push_back(callback);}
  for(auto& callback:pending)if(callback)try{callback();}
    catch(const std::exception& error){if(inputs_.logError)inputs_.logError(error);}
  const auto state=appState();const auto& missions=value::property(state,"tasks");
  if(missions.is_object())for(auto it=missions.begin();it!=missions.end();++it)
    if(ownsMission(it.key()))abortMission(it.key());
  checkShutdown();return *shutdownResult_;
}
std::shared_ptr<event_loop::Loop::KeepAlive> IndigenousBatApplication::beginBackgroundJob(){
  auto self=shared_from_this();
  return application::onOwnerAndWait<std::shared_ptr<event_loop::Loop::KeepAlive>>(
    inputs_.owner.dispatcher(),[self]{
      if(self->closing_)throw std::runtime_error("Agent application is shutting down");
      auto keep=std::make_shared<event_loop::Loop::KeepAlive>(self->inputs_.owner.hold());
      ++self->backgroundJobs_;return keep;
    });
}
void IndigenousBatApplication::finishBackgroundJob(
    const std::shared_ptr<event_loop::Loop::KeepAlive>& keep){
  keep->release();auto self=shared_from_this();
  inputs_.owner.postMission([self]{
    if(!self->backgroundJobs_)throw std::logic_error("Agent background job accounting underflow");
    --self->backgroundJobs_;self->releaseInactiveState();self->checkShutdown();
  });
}
void IndigenousBatApplication::notifyShutdownProgress(){
  if(!closing_)return;
  const auto weak=weak_from_this();
  inputs_.owner.postMission([weak]{if(auto self=weak.lock())self->checkShutdown();});
}
void IndigenousBatApplication::checkShutdown(){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("Agent shutdown progress requires owner");
  if(!shutdownResult_||shutdownFlushStarted_||backgroundJobs_||conversationJobs_)return;
  {std::lock_guard lock(capacityMutex_);if(outstanding_||active_)return;}
  shutdownFlushStarted_=true;auto self=shared_from_this();
  application::runInWorker<event_loop::Unit>(inputs_.owner,[writes=writes_]{
    if(writes)writes->flush();
    return event_loop::Unit{};
  }).then([self](const event_loop::Unit*,const std::exception_ptr& error){
    if(error)self->shutdownResult_->reject(error);
    else{
      try{self->inputs_.notifyCommandLifecycle("native-agent-shutdown","complete");}catch(...){}
      self->shutdownResult_->resolve({});
    }
  });
}
} // namespace batbox::application_bats
