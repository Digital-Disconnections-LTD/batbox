#pragma once
#include "event_loop/Loop.hpp"
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace batbox::react_scheduler {
struct Callback;
using CallbackRef=std::shared_ptr<Callback>;
struct Callback {
  std::function<CallbackRef(bool didTimeout)> run;
};
using MissionCallback=std::function<CallbackRef(bool didTimeout)>;
CallbackRef callback(MissionCallback mission);
struct Mission {
  std::uint64_t id=0;
  CallbackRef callback;
  int priorityLevel=3;
  double startTime=0,expirationTime=0,sortIndex=-1;
};
using MissionRef=std::shared_ptr<Mission>;
using TimerRef=std::shared_ptr<void>;
struct Host {
  // Same owner strand; postWork and timers must never fire inline.
  std::function<double()> now;
  std::function<void(std::function<void()>)> postWork;
  std::function<TimerRef(std::function<void()>,double)> setTimeout;
  std::function<void(const TimerRef&)> clearTimeout;
  std::function<void(const std::string&)> consoleError;
};
class Scheduler {
public:
  explicit Scheduler(Host host);
  ~Scheduler();
  Scheduler(const Scheduler&)=delete;
  Scheduler& operator=(const Scheduler&)=delete;
  static constexpr int Immediate=1,UserBlocking=2,Normal=3,Low=4,Idle=5;
  MissionRef scheduleCallback(int priority,MissionCallback mission,
    std::optional<double> delay={});
  void cancelCallback(const MissionRef& mission);
  int currentPriorityLevel()const{return currentPriorityLevel_;}
  void runWithPriority(int priority,const std::function<void()>& mission);
  void next(const std::function<void()>& mission);
  std::function<void()> wrapCallback(std::function<void()> mission);
  bool shouldYield()const;
  void requestPaint(){needsPaint_=true;}
  void forceFrameRate(double fps);
  double now()const{return host_.now();}
private:
  Host host_;
  std::vector<MissionRef> missionQueue_,timerQueue_;
  std::uint64_t missionIdCounter_=1;
  MissionRef currentMission_;
  int currentPriorityLevel_=Normal;
  bool isPerformingWork_=false,isHostCallbackScheduled_=false;
  bool isHostTimeoutScheduled_=false,needsPaint_=false;
  bool isMessageLoopRunning_=false;
  TimerRef missionTimeout_;
  std::shared_ptr<int> lifetime_=std::make_shared<int>(0);
  double frameInterval_=5,startTime_=-1;
  void advanceTimers(double currentTime);
  void handleTimeout(double currentTime);
  void performWorkUntilDeadline();
  void schedulePerformWorkUntilDeadline();
  void requestHostTimeout(double delay);
  void clearHostTimeout();
  static MissionRef peek(const std::vector<MissionRef>& heap);
  static void push(std::vector<MissionRef>& heap,const MissionRef& node);
  static MissionRef pop(std::vector<MissionRef>& heap);
};
Host indigenousHost(event_loop::Loop& loop,
  std::function<void(const std::string&)> consoleError);
} // namespace batbox::react_scheduler
