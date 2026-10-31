#include "react_scheduler/Scheduler.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <deque>
#include <iostream>

using namespace batbox;
using namespace batbox::react_scheduler;
struct Timer {
  int id;
  double at;
  bool active=true;
  std::function<void()> mission;
};
int main(){
  double time=0;
  int nextTimer=1;
  std::deque<std::function<void()>> work;
  std::vector<std::shared_ptr<Timer>> timers;
  Json events=Json::array();
  Host host{
    [&]{return time;},
    [&](std::function<void()> mission){work.push_back(std::move(mission));},
    [&](std::function<void()> mission,double delay)->TimerRef{
      auto timer=std::make_shared<Timer>(Timer{nextTimer++,time+delay,true,
        std::move(mission)});
      timers.push_back(timer);return timer;
    },
    [](const TimerRef& input){
      std::static_pointer_cast<Timer>(input)->active=false;
    },
    [&](const std::string& message){events.push_back(Json::array({"error",message}));}
  };
  Scheduler scheduler(host);
  const auto drain=[&]{
    for(int i=0;!work.empty();++i){
      if(i>50)throw std::runtime_error("unbounded scheduler work");
      auto next=std::move(work.front());work.pop_front();next();
    }
  };
  const auto advance=[&](double ms){
    time+=ms;
    for(;;){
      std::shared_ptr<Timer> due;
      for(const auto& timer:timers)
        if(timer->active&&timer->at<=time&&
            (!due||timer->at<due->at||
            (timer->at==due->at&&timer->id<due->id)))due=timer;
      if(!due)break;
      due->active=false;due->mission();drain();
    }
  };
  const auto low=scheduler.scheduleCallback(4,[&](bool late){
    events.push_back(Json::array({"low",late,scheduler.currentPriorityLevel()}));
    return callback([&](bool continuationLate){
      events.push_back(Json::array({"continuation",continuationLate}));
      return CallbackRef{};
    });
  });
  scheduler.scheduleCallback(2,[&](bool late){
    events.push_back(Json::array({"user",late,scheduler.currentPriorityLevel()}));
    return CallbackRef{};
  });
  scheduler.scheduleCallback(3,[&](bool late){
    events.push_back(Json::array({"normal",late,scheduler.currentPriorityLevel()}));
    return CallbackRef{};
  });
  scheduler.scheduleCallback(1,[&](bool late){
    events.push_back(Json::array({"immediate",late,scheduler.currentPriorityLevel()}));
    scheduler.requestPaint();return CallbackRef{};
  });
  const auto cancelled=scheduler.scheduleCallback(1,[&](bool){
    events.push_back(Json::array({"cancelled"}));return CallbackRef{};
  });
  scheduler.cancelCallback(cancelled);
  events.push_back(Json::array({"ids",low->id,cancelled->id}));
  drain();
  scheduler.scheduleCallback(3,[&](bool late){
    events.push_back(Json::array({"delay40",late}));return CallbackRef{};
  },40);
  scheduler.scheduleCallback(1,[&](bool late){
    events.push_back(Json::array({"delay20",late}));return CallbackRef{};
  },20);
  advance(20);advance(20);
  std::function<void()> wrapped;
  scheduler.runWithPriority(1,[&]{
    events.push_back(Json::array({"priority",scheduler.currentPriorityLevel()}));
    scheduler.next([&]{events.push_back(Json::array({"next",
      scheduler.currentPriorityLevel()}));});
    wrapped=scheduler.wrapCallback([&]{events.push_back(Json::array({"wrapped",
      scheduler.currentPriorityLevel()}));});
  });
  scheduler.runWithPriority(5,wrapped);
  events.push_back(Json::array({"restored",scheduler.currentPriorityLevel()}));
  scheduler.forceFrameRate(-1);scheduler.forceFrameRate(125);
  events.push_back(Json::array({"yield",scheduler.shouldYield()}));
  std::cout<<events.dump()<<'\n';
}
