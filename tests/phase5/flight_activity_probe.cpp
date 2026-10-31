#include "flights/Activity.hpp"
#include "utils/envUtils.hpp"
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace batbox;
namespace {
struct Timer {
  int id;double due,delay;bool repeat,active=true;
  std::function<void()> callback;
};
Json run(const Json& item){
  double now=0;int nextId=1;Json env=item.value("env",Json());
  Json logs=Json::array(),out=Json::array();
  std::vector<std::shared_ptr<Timer>> timers;
  std::vector<std::function<void()>> cleanups;
  flights::ActivityServices s;
  s.now=[&]{return now;};s.sendKeepalives=[&]{return isEnvTruthy(env);};
  auto schedule=[&](double delay,std::function<void()> callback,bool repeat){
    auto timer=std::make_shared<Timer>(Timer{nextId++,now+delay,delay,repeat,true,std::move(callback)});
    timers.push_back(timer);
    return cleanup::ShutdownTimer{[timer]{timer->active=false;},[]{}};
  };
  s.setInterval=[&](double delay,std::function<void()> callback){return schedule(delay,std::move(callback),true);};
  s.setTimeout=[&](double delay,std::function<void()> callback){return schedule(delay,std::move(callback),false);};
  s.registerCleanup=[&](std::function<void()> cb){logs.push_back(Json::array({"registerCleanup"}));cleanups.push_back(std::move(cb));};
  s.diagnostic=[&](const std::string& level,const std::string& event,Json data){
    logs.push_back(Json::array({level,event,std::move(data)}));};
  flights::Activity activity(s);
  for(const auto& step:item.at("steps")){
    const auto op=step.at("op").get<std::string>();
    if(op=="register")activity.registerFlightActivityCallback([&,step]{
      logs.push_back(Json::array({"callback",step.at("name")}));
      if(step.value("unregister",false))activity.unregisterFlightActivityCallback();
    });
    else if(op=="unregister")activity.unregisterFlightActivityCallback();
    else if(op=="start")activity.startFlightActivity(step.at("reason"));
    else if(op=="stop")activity.stopFlightActivity(step.at("reason"));
    else if(op=="signal")activity.sendFlightActivitySignal();
    else if(op=="env")env=step.at("value");
    else if(op=="cleanup")for(auto& cb:cleanups)cb();
    else if(op=="advance"){
      const auto end=now+step.at("ms").get<double>();int iterations=0;
      while(true){
        std::shared_ptr<Timer> next;
        for(const auto& t:timers)if(t->active&&t->due<=end&&
          (!next||t->due<next->due||(t->due==next->due&&t->id<next->id)))next=t;
        if(!next)break;
        if(++iterations>1000)throw std::runtime_error("timer fixture runaway");
        now=next->due;if(next->repeat)next->due+=next->delay;else next->active=false;
        next->callback();
      }
      now=end;
    }else throw std::runtime_error("unknown activity operation");
    Json activeTimers=Json::array();
    for(const auto& t:timers)if(t->active)activeTimers.push_back(Json::array({t->id,t->due,t->repeat}));
    out.push_back(Json{{"now",now},{"active",activity.isFlightActivityTrackingActive()},
      {"timers",activeTimers},{"logs",logs}});
  }
  return out;
}
}
int main(){
  const std::string input(std::istreambuf_iterator<char>{std::cin},{});
  const auto cases=value::parse(input);Json out=Json::array();
  for(const auto& item:cases)out.push_back(run(item));
  std::cout<<value::stringify(out)<<'\n';
}
