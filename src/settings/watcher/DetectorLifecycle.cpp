#include "settings/watcher/DetectorLifecycle.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::settings::watcher {
namespace {
double selected(std::optional<double> overrideValue,double sourceDefault){
  return overrideValue.value_or(sourceDefault);
}
}
DetectorLifecycle::DetectorLifecycle(DetectorServices services):
    services_(std::move(services)){
  if(!services_.owner.valid()||!services_.isRemoteMode||
      !services_.startMdmPoll||!services_.stopMdmPoll||
      !services_.registerCleanup||!services_.getWatchTargets||
      !services_.openWatcher||!services_.debug||
      !services_.clearPendingDeletions||!services_.clearInternalWrites||
      !services_.clearSignal||!services_.setDeletionGrace)
    throw std::invalid_argument("Missing settings detector lifecycle binding");
}
DetectorLifecycle::~DetectorLifecycle(){alive_->store(false);}
std::future<void> DetectorLifecycle::readyFuture(){
  std::promise<void> promise;promise.set_value();return promise.get_future();
}
std::future<void> DetectorLifecycle::initialize(){
  if(!services_.owner.isOwnerThread())
    throw std::logic_error("Settings detector owner thread required");
  if(services_.isRemoteMode())return readyFuture();
  if(initialized_||disposed_)return readyFuture();
  auto promise=std::make_shared<std::promise<void>>();
  auto future=promise->get_future();
  initialized_=true;
  try{
    services_.startMdmPoll(selected(overrides_?
      overrides_->mdmPollInterval:std::nullopt,1800000));
    services_.registerCleanup([this](){return dispose();});
    services_.getWatchTargets([this,promise,alive=alive_,
        owner=services_.owner](
      std::optional<persistence::WatchTargets> targets,
      std::exception_ptr error){
    if(!alive->load()){promise->set_value();return;}
    auto continuation=[this,promise,alive,targets=std::move(targets),
        error]()mutable{
      try{
        if(!alive->load()){promise->set_value();return;}
        if(error)std::rethrow_exception(error);
        if(!targets)throw std::logic_error("Settings watch targets missing");
        if(!disposed_&&!targets->dirs.empty()){
          std::string files;
          for(std::size_t i=0;i<targets->settingsFiles.size();++i){
            if(i)files+=", ";
            files+=targets->settingsFiles[i];
          }
          services_.debug("Watching for changes in setting files "+files+
            "..."+(targets->dropInDir?" and drop-in directory "+
              *targets->dropInDir:std::string{}));
          Options options;
          options.stabilityThresholdMs=selected(overrides_?
            overrides_->stabilityThreshold:std::nullopt,1000);
          options.pollIntervalMs=selected(overrides_?
            overrides_->pollInterval:std::nullopt,500);
          watcherClose_=services_.openWatcher(*targets,options,
            services_.events);
        }
        promise->set_value();
      }catch(...){promise->set_exception(std::current_exception());}
    };
    if(owner.isOwnerThread())
      owner.queueMicrotask(std::move(continuation));
    else owner.postMission([owner,
        continuation=std::move(continuation)]()mutable{
      owner.queueMicrotask(std::move(continuation));
    });
    });
  }catch(...){promise->set_exception(std::current_exception());}
  return future;
}
std::future<void> DetectorLifecycle::closeWatcher(){
  if(!watcherClose_)return readyFuture();
  auto close=std::move(*watcherClose_);watcherClose_.reset();
  auto promise=std::make_shared<std::promise<void>>();
  auto future=promise->get_future();
  close([promise](){promise->set_value();});
  return future;
}
std::future<void> DetectorLifecycle::dispose(){
  if(!services_.owner.isOwnerThread())
    throw std::logic_error("Settings detector owner thread required");
  disposed_=true;
  services_.stopMdmPoll();
  services_.clearPendingDeletions();
  services_.clearInternalWrites();
  services_.clearSignal();
  return closeWatcher();
}
std::future<void> DetectorLifecycle::resetForTesting(
    std::optional<TimingOverrides> overrides){
  if(!services_.owner.isOwnerThread())
    throw std::logic_error("Settings detector owner thread required");
  services_.stopMdmPoll();
  services_.clearPendingDeletions();
  initialized_=false;disposed_=false;
  overrides_=std::move(overrides);
  services_.setDeletionGrace(selected(overrides_?
    overrides_->deletionGrace:std::nullopt,1700));
  return closeWatcher();
}
WatcherClose openIndigenousWatcher(const persistence::WatchTargets& targets,
    const event_loop::Loop::Dispatcher& owner,const Events& events,
    const Options& options){
  auto watcher=std::make_shared<IndigenousWatcher>(targets,owner,events,options);
  return [watcher=std::move(watcher)](std::function<void()> done){
    watcher->close();
    done();
  };
}
std::function<void(std::function<void(
    std::optional<persistence::WatchTargets>,std::exception_ptr)>)>
bindIndigenousWatchTargets(Paths& paths,
    const event_loop::Loop::Dispatcher& owner){
  return [&paths,owner](auto done){
    owner.queueMicrotask([&paths,owner,done=std::move(done)]()mutable{
      std::optional<persistence::WatchTargets> targets;
      std::exception_ptr error;
      try{
        targets=persistence::getWatchTargets(
          persistence::bindWatchPaths(paths),
          persistence::indigenousWatchStatReader());
      }catch(...){error=std::current_exception();}
      owner.queueMicrotask([done=std::move(done),
          targets=std::move(targets),error]()mutable{
        done(std::move(targets),error);
      });
    });
  };
}
}
