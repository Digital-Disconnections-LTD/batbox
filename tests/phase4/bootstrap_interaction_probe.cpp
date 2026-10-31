#include "bootstrap/Interaction.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <vector>

int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    event_loop::Loop loop;
    Json trace=Json::array();
    struct Timer {int id;double due;bool active;
      std::function<void()> callback;};
    std::vector<Timer> timers;
    int nextTimer=1;
    double time=0;
    bool initializing=true,waitResolved=false;
    bootstrap::InteractionServices services;
    services.nowMs=[&]{
      if(initializing)return 1000.0;
      trace.push_back("now");return 2000.0;};
    services.setTimeout=[&](double delay,std::function<void()> callback){
      const auto id=nextTimer++;
      timers.push_back({id,time+delay,true,std::move(callback)});
      trace.push_back(Json::array({"timer",id,delay}));
      return bootstrap::InteractionTimer{
        [&,id]{trace.push_back(Json::array({"cancel",id}));
          for(auto& timer:timers)if(timer.id==id)timer.active=false;},
        [&,id]{trace.push_back(Json::array({"unref",id}));}};
    };
    auto state=bootstrap::makeInteractionState(services);
    initializing=false;
    auto advance=[&](double interval){
      const auto target=time+interval;
      for(;;){
        std::optional<std::size_t> due;
        for(std::size_t i=0;i<timers.size();++i)
          if(timers[i].active&&timers[i].due<=target&&
              (!due||timers[i].due<timers[*due].due||
                (timers[i].due==timers[*due].due&&
                  timers[i].id<timers[*due].id)))due=i;
        if(!due)break;
        time=timers[*due].due;
        timers[*due].active=false;
        const auto id=timers[*due].id;
        auto callback=timers[*due].callback;
        trace.push_back(Json::array({"fire",id}));
        callback();loop.runReady();
      }
      time=target;
    };
    if(test.contains("ops"))for(const auto& op:test["ops"]){
      const auto kind=op.value("kind",std::string());
      if(kind=="update")bootstrap::updateLastInteractionTime(state,services,
        op.value("immediate",false));
      else if(kind=="flush")bootstrap::flushInteractionTime(state,services);
      else if(kind=="mark")bootstrap::markScrollActivity(state,services);
      else if(kind=="wait"){
        bootstrap::waitForScrollIdle(loop,state,services).then(
          [&](const event_loop::Unit*,const std::exception_ptr& error){
            if(error)std::rethrow_exception(error);
            waitResolved=true;});
        loop.runReady();
      }else if(kind=="advance")advance(op["ms"].get<double>());
      else throw std::runtime_error("Unknown interaction operation");
    }
    std::cout<<value::stringify(Json{{"last",
      bootstrap::getLastInteractionTime(state)},
      {"draining",bootstrap::getIsScrollDraining(state)},
      {"waitResolved",waitResolved},{"trace",trace}})<<'\n';
  }
}
