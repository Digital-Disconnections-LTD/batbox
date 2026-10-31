#include "locking/Core.hpp"
#include "locking/Async.hpp"
#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace batbox::locking {
namespace {
std::vector<double> retryDelays(const RetryOptions& options){
  if(options.minTimeoutMs>options.maxTimeoutMs)
    throw std::invalid_argument("minTimeout is greater than maxTimeout");
  std::vector<double> delays;
  static thread_local std::mt19937_64 random(std::random_device{}());
  for(int index=0;index<options.retries;++index){
    const double multiplier=options.randomize
      ?std::uniform_real_distribution<double>(1,2)(random):1;
    delays.push_back(std::min(std::round(multiplier*
      options.minTimeoutMs*std::pow(options.factor,index)),
      options.maxTimeoutMs));
  }
  std::sort(delays.begin(),delays.end());
  return delays;
}
template<class T>event_loop::Deferred<T> rejected(event_loop::Loop& loop,
    std::exception_ptr error){
  event_loop::Deferred<T> output(loop);output.reject(error);return output;
}
}
Manager::Manager(event_loop::Loop& loop):core_(std::make_shared<Core>(loop)){}
Manager::~Manager(){if(core_)core_->shutdown();}
event_loop::Deferred<Release> Manager::lock(const std::string& file,
    Options options){
  auto core=core_;
  if(!core->loop.isOwnerThread())
    throw std::logic_error("Directory lock requires owner loop");
  options=file_ops::normalize(std::move(options));
  event_loop::Deferred<Release> output(core->loop);
  runJob<std::string>(core->loop,[file,options]{
    return file_ops::canonical(file,options.realpath);
  }).then([core,options,output](const std::string* canonical,
      const std::exception_ptr& error){
    if(error){output.reject(error);return;}
    std::vector<double> delays;
    try{delays=retryDelays(options.retry);}
    catch(...){output.reject(std::current_exception());return;}
    auto path=std::make_shared<std::string>(*canonical);
    auto attempt=std::make_shared<std::function<void(int)>>();
    *attempt=[core,options,output,path,attempt,
        delays=std::move(delays)](int count){
      if(!core->alive){*attempt={};output.reject(std::make_exception_ptr(
        std::runtime_error("Lock manager closed")));return;}
      const auto precision=core->precision;
      runJob<file_ops::Acquired>(core->loop,[path,options,precision]{
        return file_ops::acquire(*path,options,precision);
      }).then([core,options,output,path,attempt,count,delays](
          const file_ops::Acquired* acquired,
          const std::exception_ptr& error){
        if(error){
          if(count<static_cast<int>(delays.size())){
            core->loop.setTimeout(delays[count],
              [attempt,count]{(*attempt)(count+1);});
          }else{*attempt={};output.reject(error);}
          return;
        }
        *attempt={};
        if(!core->alive){
          try{file_ops::remove(acquired->lockPath);}catch(...){}
          output.reject(std::make_exception_ptr(
            std::runtime_error("Lock manager closed")));
          return;
        }
        auto owned=std::make_shared<Owned>();
        owned->canonical=acquired->canonical;
        owned->lockPath=acquired->lockPath;
        owned->mtimeMs=acquired->mtimeMs;
        owned->precision=acquired->precision;
        owned->lastUpdateMs=file_ops::nowMs();
        owned->options=options;
        core->precision=acquired->precision;
        core->locks[owned->canonical]=owned;
        core->schedule(owned);
        std::weak_ptr<Owned> weak=owned;
        output.resolve([core,weak](){
          const auto state=weak.lock();
          if(!state||state->released)return rejected<event_loop::Unit>(
            core->loop,std::make_exception_ptr(Error("ERELEASED",
              "Lock is already released")));
          return core->unlockCanonical(state->canonical);
        });
      });
    };
    (*attempt)(0);
  });
  return output;
}
event_loop::Deferred<event_loop::Unit> Manager::unlock(
    const std::string& file,Options options){
  auto core=core_;
  if(!core->loop.isOwnerThread())
    throw std::logic_error("Directory unlock requires owner loop");
  event_loop::Deferred<event_loop::Unit> output(core->loop);
  runJob<std::string>(core->loop,[file,options]{
    return file_ops::canonical(file,options.realpath);
  }).then([core,output](const std::string* canonical,
      const std::exception_ptr& error){
    if(error){output.reject(error);return;}
    core->unlockCanonical(*canonical).then([output](
        const event_loop::Unit*,const std::exception_ptr& unlockError){
      if(unlockError)output.reject(unlockError);
      else output.resolve({});
    });
  });
  return output;
}
event_loop::Deferred<bool> Manager::check(const std::string& file,
    Options options){
  auto core=core_;
  if(!core->loop.isOwnerThread())
    throw std::logic_error("Directory lock check requires owner loop");
  options=file_ops::normalize(std::move(options));
  return runJob<bool>(core->loop,[file,options]{
    const auto path=file_ops::canonical(file,options.realpath);
    return file_ops::check(path,options);
  });
}
std::function<void()> Manager::lockSync(const std::string& file,
    Options options){
  auto core=core_;
  if(!core->loop.isOwnerThread())
    throw std::logic_error("Sync directory lock requires owner loop");
  if(options.retry.retries>0)
    throw Error("ESYNC","Cannot use retries with the sync api");
  options=file_ops::normalize(std::move(options));
  const auto path=file_ops::canonical(file,options.realpath);
  const auto acquired=file_ops::acquire(path,options,core->precision);
  auto owned=std::make_shared<Owned>();
  owned->canonical=acquired.canonical;
  owned->lockPath=acquired.lockPath;
  owned->mtimeMs=acquired.mtimeMs;
  owned->precision=acquired.precision;
  owned->lastUpdateMs=file_ops::nowMs();
  owned->options=options;
  core->precision=acquired.precision;
  core->locks[path]=owned;
  core->schedule(owned);
  std::weak_ptr<Owned> weak=owned;
  return [core,weak]{
    const auto state=weak.lock();
    if(!state||state->released)
      throw Error("ERELEASED","Lock is already released");
    core->unlockCanonicalSync(state->canonical);
  };
}
void Manager::unlockSync(const std::string& file,Options options){
  auto core=core_;
  if(!core->loop.isOwnerThread())
    throw std::logic_error("Sync directory unlock requires owner loop");
  core->unlockCanonicalSync(file_ops::canonical(file,options.realpath));
}
bool Manager::checkSync(const std::string& file,Options options){
  auto core=core_;
  if(!core->loop.isOwnerThread())
    throw std::logic_error("Sync directory check requires owner loop");
  options=file_ops::normalize(std::move(options));
  return file_ops::check(file_ops::canonical(file,options.realpath),options);
}
event_loop::Deferred<event_loop::Unit> Core::unlockCanonical(
    const std::string& canonical){
  const auto found=locks.find(canonical);
  if(found==locks.end())return rejected<event_loop::Unit>(loop,
    std::make_exception_ptr(Error("ENOTACQUIRED",
      "Lock is not acquired/owned by you")));
  const auto owned=found->second;
  owned->timer.cancel();owned->released=true;locks.erase(found);
  return runJob<event_loop::Unit>(loop,[path=owned->lockPath]{
    file_ops::remove(path);return event_loop::Unit{};
  });
}
void Core::unlockCanonicalSync(const std::string& canonical){
  const auto found=locks.find(canonical);
  if(found==locks.end())throw Error("ENOTACQUIRED",
    "Lock is not acquired/owned by you");
  const auto owned=found->second;
  owned->timer.cancel();owned->released=true;locks.erase(found);
  file_ops::remove(owned->lockPath);
}
void Core::shutdown(){
  if(!alive)return;
  alive=false;
  for(auto& [file,owned]:locks){
    (void)file;
    owned->released=true;owned->timer.cancel();
    try{file_ops::remove(owned->lockPath);}catch(...){}
  }
  locks.clear();
}
}
