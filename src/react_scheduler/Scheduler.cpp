#include "react_scheduler/Scheduler.hpp"
#include <cmath>
#include <stdexcept>

namespace batbox::react_scheduler {
CallbackRef callback(MissionCallback mission){
  return std::make_shared<Callback>(Callback{std::move(mission)});
}
Scheduler::Scheduler(Host host):host_(std::move(host)){
  if(!host_.now||!host_.postWork||!host_.setTimeout||
      !host_.clearTimeout||!host_.consoleError)
    throw std::invalid_argument("React scheduler host imports required");
}
Scheduler::~Scheduler(){
  lifetime_.reset();
  if(missionTimeout_)host_.clearTimeout(missionTimeout_);
}
void Scheduler::advanceTimers(double currentTime){
  for(auto timer=peek(timerQueue_);timer;timer=peek(timerQueue_)){
    if(!timer->callback)pop(timerQueue_);
    else if(timer->startTime<=currentTime){
      pop(timerQueue_);
      timer->sortIndex=timer->expirationTime;
      push(missionQueue_,timer);
    }else break;
  }
}
void Scheduler::handleTimeout(double currentTime){
  isHostTimeoutScheduled_=false;
  missionTimeout_.reset();
  advanceTimers(currentTime);
  if(!isHostCallbackScheduled_){
    if(peek(missionQueue_)){
      isHostCallbackScheduled_=true;
      if(!isMessageLoopRunning_){
        isMessageLoopRunning_=true;
        schedulePerformWorkUntilDeadline();
      }
    }else if(const auto firstTimer=peek(timerQueue_))
      requestHostTimeout(firstTimer->startTime-currentTime);
  }
}
bool Scheduler::shouldYield()const{
  if(needsPaint_)return true;
  return !(host_.now()-startTime_<frameInterval_);
}
void Scheduler::performWorkUntilDeadline(){
  needsPaint_=false;
  if(!isMessageLoopRunning_)return;
  auto currentTime=host_.now();
  startTime_=currentTime;
  bool hasMoreWork=true;
  try{
    isHostCallbackScheduled_=false;
    if(isHostTimeoutScheduled_){
      isHostTimeoutScheduled_=false;
      clearHostTimeout();
    }
    isPerformingWork_=true;
    const auto previousPriorityLevel=currentPriorityLevel_;
    try{
      advanceTimers(currentTime);
      for(currentMission_=peek(missionQueue_);
          currentMission_&&
          !(currentMission_->expirationTime>currentTime&&shouldYield());){
        const auto mission=currentMission_;
        const auto missionCallback=mission->callback;
        if(missionCallback&&missionCallback->run){
          mission->callback.reset();
          currentPriorityLevel_=mission->priorityLevel;
          auto continuation=missionCallback->run(
            mission->expirationTime<=currentTime);
          currentTime=host_.now();
          if(continuation&&continuation->run){
            mission->callback=std::move(continuation);
            advanceTimers(currentTime);
            hasMoreWork=true;
            break;
          }
          if(mission==peek(missionQueue_))pop(missionQueue_);
          advanceTimers(currentTime);
        }else pop(missionQueue_);
        currentMission_=peek(missionQueue_);
      }
      if(currentMission_)hasMoreWork=true;
      else if(const auto firstTimer=peek(timerQueue_)){
        requestHostTimeout(firstTimer->startTime-currentTime);
        hasMoreWork=false;
      }else hasMoreWork=false;
    }catch(...){
      currentMission_.reset();
      currentPriorityLevel_=previousPriorityLevel;
      isPerformingWork_=false;
      throw;
    }
    currentMission_.reset();
    currentPriorityLevel_=previousPriorityLevel;
    isPerformingWork_=false;
  }catch(...){
    if(hasMoreWork)schedulePerformWorkUntilDeadline();
    else isMessageLoopRunning_=false;
    throw;
  }
  if(hasMoreWork)schedulePerformWorkUntilDeadline();
  else isMessageLoopRunning_=false;
}
void Scheduler::schedulePerformWorkUntilDeadline(){
  const std::weak_ptr<int> weak=lifetime_;
  host_.postWork([this,weak]{
    if(weak.lock())performWorkUntilDeadline();
  });
}
void Scheduler::requestHostTimeout(double delay){
  const std::weak_ptr<int> weak=lifetime_;
  missionTimeout_=host_.setTimeout([this,weak]{
    if(weak.lock())handleTimeout(host_.now());
  },delay);
}
void Scheduler::clearHostTimeout(){
  if(missionTimeout_)host_.clearTimeout(missionTimeout_);
  missionTimeout_.reset();
}
void Scheduler::cancelCallback(const MissionRef& mission){
  if(mission)mission->callback.reset();
}
void Scheduler::forceFrameRate(double fps){
  if(fps<0||fps>125)host_.consoleError(
    "forceFrameRate takes a positive int between 0 and 125, "
    "forcing frame rates higher than 125 fps is not supported");
  else frameInterval_=fps>0?std::floor(1000/fps):5;
}
void Scheduler::runWithPriority(int priority,
    const std::function<void()>& mission){
  if(priority<Immediate||priority>Idle)priority=Normal;
  const auto previous=currentPriorityLevel_;
  currentPriorityLevel_=priority;
  try{mission();}catch(...){currentPriorityLevel_=previous;throw;}
  currentPriorityLevel_=previous;
}
void Scheduler::next(const std::function<void()>& mission){
  const auto priority=currentPriorityLevel_<=Normal?
    Normal:currentPriorityLevel_;
  runWithPriority(priority,mission);
}
std::function<void()> Scheduler::wrapCallback(std::function<void()> mission){
  const auto parentPriority=currentPriorityLevel_;
  const std::weak_ptr<int> weak=lifetime_;
  return [this,weak,parentPriority,mission=std::move(mission)]{
    if(!weak.lock())return;
    const auto previous=currentPriorityLevel_;
    currentPriorityLevel_=parentPriority;
    try{mission();}catch(...){currentPriorityLevel_=previous;throw;}
    currentPriorityLevel_=previous;
  };
}
MissionRef Scheduler::scheduleCallback(int priority,MissionCallback mission,
    std::optional<double> delay){
  const auto currentTime=host_.now();
  const auto startTime=delay&&*delay>0?currentTime+*delay:currentTime;
  double timeout=5000;
  switch(priority){
    case Immediate:timeout=-1;break;
    case UserBlocking:timeout=250;break;
    case Idle:timeout=1073741823;break;
    case Low:timeout=10000;break;
    default:break;
  }
  const auto expirationTime=startTime+timeout;
  auto scheduled=std::make_shared<Mission>(Mission{missionIdCounter_++,
    callback(std::move(mission)),priority,startTime,expirationTime,-1});
  if(startTime>currentTime){
    scheduled->sortIndex=startTime;
    push(timerQueue_,scheduled);
    if(!peek(missionQueue_)&&scheduled==peek(timerQueue_)){
      if(isHostTimeoutScheduled_)clearHostTimeout();
      else isHostTimeoutScheduled_=true;
      requestHostTimeout(startTime-currentTime);
    }
  }else{
    scheduled->sortIndex=expirationTime;
    push(missionQueue_,scheduled);
    if(!isHostCallbackScheduled_&&!isPerformingWork_){
      isHostCallbackScheduled_=true;
      if(!isMessageLoopRunning_){
        isMessageLoopRunning_=true;
        schedulePerformWorkUntilDeadline();
      }
    }
  }
  return scheduled;
}
} // namespace batbox::react_scheduler
