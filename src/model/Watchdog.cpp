#include "runtime/AsyncLaunch.hpp"
#include "model/Watchdog.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <optional>
#include <utility>

namespace batbox::model {
namespace {
std::chrono::steady_clock::duration duration(double ms){
  return std::chrono::duration_cast<std::chrono::steady_clock::duration>(
    std::chrono::duration<double,std::milli>(ms));
}
// Whole milliseconds left until due, at least one so a re-armed timer cannot
// fire early and spin.
std::optional<double> remainingMs(std::chrono::steady_clock::time_point due){
  const auto now=std::chrono::steady_clock::now();
  if(now>=due)return std::nullopt;
  return std::max(1.0,std::ceil(std::chrono::duration<double,std::milli>(due-now).count()));
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
    // Each reset only records the time; the scheduler is reached again only
    // when a timer has already fired.
    bool armWarning=false,armTimeout=false;
    std::uint64_t generation;
    {
      std::lock_guard lock(mutex_);
      if(stopped_)return;
      generation=generation_;
      lastReset_=std::chrono::steady_clock::now();
      armed_=true;warned_=false;
      armWarning=!warningPending_;armTimeout=!timeoutPending_;
      warningPending_=timeoutPending_=true;
    }
    if(armWarning)scheduleWarning(settings_.warningMs,generation);
    if(armTimeout)scheduleTimeout(settings_.timeoutMs,generation);
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
void StreamWatchdog::scheduleWarning(double delayMs,std::uint64_t generation){
  auto cancel=scheduler_(delayMs,[this,generation]{onWarningTimer(generation);});
  {
    std::lock_guard lock(mutex_);
    if(!stopped_&&generation_==generation){cancelWarning_=std::move(cancel);return;}
  }
  if(cancel)cancel();
}
void StreamWatchdog::scheduleTimeout(double delayMs,std::uint64_t generation){
  auto cancel=scheduler_(delayMs,[this,generation]{onTimeoutTimer(generation);});
  {
    std::lock_guard lock(mutex_);
    if(!stopped_&&generation_==generation){cancelTimeout_=std::move(cancel);return;}
  }
  if(cancel)cancel();
}
void StreamWatchdog::onWarningTimer(std::uint64_t generation){
  std::optional<double> rest;
  {
    std::lock_guard lock(mutex_);
    if(stopped_||generation_!=generation)return;
    rest=warned_?std::nullopt:remainingMs(lastReset_+duration(settings_.warningMs));
    if(!rest)warningPending_=false;
  }
  if(rest)scheduleWarning(*rest,generation);
  else fireWarning(generation);
}
void StreamWatchdog::onTimeoutTimer(std::uint64_t generation){
  std::optional<double> rest;
  {
    std::lock_guard lock(mutex_);
    if(stopped_||generation_!=generation)return;
    rest=remainingMs(lastReset_+duration(settings_.timeoutMs));
    if(!rest)timeoutPending_=false;
  }
  if(rest)scheduleTimeout(*rest,generation);
  else fireTimeout(generation);
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
