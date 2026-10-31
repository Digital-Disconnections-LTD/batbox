#include "runtime/AsyncLaunch.hpp"
#include "model/Watchdog.hpp"
#include <algorithm>
#include <chrono>
#include <utility>

namespace batbox::model {
namespace {
std::chrono::steady_clock::duration duration(double ms){
  return std::chrono::duration_cast<std::chrono::steady_clock::duration>(
    std::chrono::duration<double,std::milli>(ms));
}
}
StreamWatchdog::StreamWatchdog(WatchdogSettings settings,
    std::function<void()> abort,std::function<void()> warning,
    std::function<void()> timeout,TimerScheduler scheduler):
  settings_(settings),abort_(std::move(abort)),warning_(std::move(warning)),
  timeout_(std::move(timeout)),scheduler_(std::move(scheduler)){
  if(settings_.enabled&&!scheduler_)timer_=::batbox::runtime::contextThread([this]{run();});
}
StreamWatchdog::~StreamWatchdog(){clear();}
void StreamWatchdog::reset(){
  if(!settings_.enabled)return;
  if(scheduler_){
    std::function<void()> oldWarning,oldTimeout;
    std::uint64_t generation;
    {
      std::lock_guard lock(mutex_);
      if(stopped_)return;
      ++generation_;generation=generation_;
      armed_=true;warned_=false;
      oldWarning=std::move(cancelWarning_);
      oldTimeout=std::move(cancelTimeout_);
    }
    if(oldWarning)oldWarning();
    if(oldTimeout)oldTimeout();
    auto cancelWarning=scheduler_(settings_.warningMs,[this,generation]{
      fireWarning(generation);});
    auto cancelTimeout=scheduler_(settings_.timeoutMs,[this,generation]{
      fireTimeout(generation);});
    bool cancelled=false;
    {
      std::lock_guard lock(mutex_);
      if(stopped_||generation_!=generation)cancelled=true;
      else{cancelWarning_=cancelWarning;cancelTimeout_=cancelTimeout;}
    }
    if(cancelled){if(cancelWarning)cancelWarning();if(cancelTimeout)cancelTimeout();}
    return;
  }
  {
    std::lock_guard lock(mutex_);
    if(stopped_)return;
    ++generation_;
    armed_=true;warned_=false;
    const auto now=std::chrono::steady_clock::now();
    warningDeadline_=now+duration(settings_.warningMs);
    deadline_=now+duration(settings_.timeoutMs);
  }
  changed_.notify_all();
}
void StreamWatchdog::clear() noexcept{
  std::function<void()> warning,timeout;
  {
    std::lock_guard lock(mutex_);
    stopped_=true;armed_=false;
    ++generation_;
    warning=std::move(cancelWarning_);
    timeout=std::move(cancelTimeout_);
  }
  changed_.notify_all();
  try{if(warning)warning();}catch(...){}
  try{if(timeout)timeout();}catch(...){}
  if(timer_.joinable())timer_.join();
}
bool StreamWatchdog::aborted()const{
  std::lock_guard lock(mutex_);return aborted_;
}
void StreamWatchdog::fireWarning(std::uint64_t generation){
  {
    std::lock_guard lock(mutex_);
    if(stopped_||warned_||generation_!=generation)return;
    warned_=true;
  }
  try{if(warning_)warning_();}catch(...){}
}
void StreamWatchdog::fireTimeout(std::uint64_t generation){
  {
    std::lock_guard lock(mutex_);
    if(stopped_||!armed_||generation_!=generation)return;
    aborted_=true;armed_=false;
  }
  try{if(timeout_)timeout_();}catch(...){}
  try{if(abort_)abort_();}catch(...){}
}
void StreamWatchdog::run(){
  std::unique_lock lock(mutex_);
  while(!stopped_){
    changed_.wait(lock,[this]{return stopped_||armed_;});
    if(stopped_)break;
    const auto generation=generation_;
    const auto deadline=warned_?deadline_:
      std::min(warningDeadline_,deadline_);
    if(changed_.wait_until(lock,deadline,[this,generation]{
        return stopped_||generation_!=generation||!armed_;}))continue;
    const bool warning=!warned_&&warningDeadline_<=deadline_;
    lock.unlock();
    if(warning)fireWarning(generation);else fireTimeout(generation);
    lock.lock();
  }
}
}
