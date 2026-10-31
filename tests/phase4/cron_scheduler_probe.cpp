#include "cron/CronScheduler.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/Platform.hpp"
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

using batbox::Json;
using namespace batbox::cron;
using namespace batbox::event_loop;
namespace {
CronMissionRef parseMission(const Json& value){
  auto mission=std::make_shared<CronMission>();
  mission->id=value.at("id").get<std::string>();
  mission->cron=value.at("cron").get<std::string>();
  mission->poetry=value.at("prompt").get<std::string>();
  mission->createdAt=value.at("createdAt").get<double>();
  if(value.contains("lastFiredAt"))mission->lastFiredAt=
    value.at("lastFiredAt").get<double>();
  if(value.contains("recurring"))mission->recurring=
    value.at("recurring").get<bool>();
  if(value.contains("permanent"))mission->permanent=
    value.at("permanent").get<bool>();
  return mission;
}
std::vector<CronMissionRef> parseMissions(const Json& values){
  std::vector<CronMissionRef> out;
  for(const auto& value:values)out.push_back(parseMission(value));
  return out;
}
struct Timer final:SchedulerTimer {
  double delay;
  bool active=true;
  std::function<void()> callback;
  Json* trace;
  Timer(double d,std::function<void()> fn,Json& t):delay(d),
    callback(std::move(fn)),trace(&t){}
  void cancel()override{
    active=false;trace->push_back("clear:"+batbox::value::numberToString(delay));
  }
};
struct Watcher final:SchedulerWatcher {
  std::function<void()> add,change,unlink;
  Json* trace;
  explicit Watcher(Json& t):trace(&t){}
  void close()override{trace->push_back("closeWatch");}
};
struct State {
  double now=0;
  Json fileMissions=Json::array(),flightMissions=Json::array();
  bool enabled=false,loading=false,killed=false;
  std::vector<bool> lockSequence;
  std::size_t lockIndex=0;
  Json trace=Json::array(),fires=Json::array(),missed=Json::array();
  std::vector<std::shared_ptr<Timer>> timers;
  std::shared_ptr<Watcher> watcher;
};
std::string yes(bool value){return value?"true":"false";}
Json run(const Json& input){
  Loop loop;
  auto state=std::make_shared<State>();
  state->now=input.value("now",0.0);
  state->fileMissions=input.value("fileTasks",Json::array());
  state->flightMissions=input.value("sessionTasks",Json::array());
  state->enabled=input.value("enabled",false);
  state->loading=input.value("loading",false);
  state->killed=input.value("killed",false);
  for(const auto& item:input.value("lockSequence",Json::array({true})))
    state->lockSequence.push_back(item.get<bool>());
  SchedulerOptions options;
  options.onFire=[state](const std::string& poetry){
    state->trace.push_back("fire:"+poetry);state->fires.push_back(poetry);
  };
  options.isLoading=[state]{state->trace.push_back("loading");
    return state->loading;};
  options.assistantMode=input.value("assistantMode",false);
  if(input.contains("dir"))options.dir=input.at("dir").get<std::string>();
  if(input.value("onFireTask",false))options.onFireMission=[state](
    const CronMissionRef& mission){state->trace.push_back("fireTask:"+mission->id);
      state->fires.push_back(mission->id);};
  if(input.value("onMissed",false))options.onMissed=[state,
    mutate=input.value("mutateMissed",false)](
    std::vector<CronMissionRef>& missions){
      Json ids=Json::array();std::string joined;
      for(const auto& mission:missions){if(!joined.empty())joined+=",";
        joined+=mission->id;ids.push_back(mission->id);}
      state->trace.push_back("missed:"+joined);
      state->missed.push_back(std::move(ids));
      if(mutate&&!missions.empty())missions[0]->id="changed";
    };
  if(input.contains("filterIds"))options.filter=[state,
    ids=input.at("filterIds")](const CronMissionRef& mission){
      state->trace.push_back("filter:"+mission->id);
      for(const auto& id:ids)if(id==mission->id)return true;
      return false;
    };
  options.getJitterConfig=[state,input]{
    state->trace.push_back("jitter");
    auto config=defaultCronJitterConfig;
    config.recurringMaxAgeMs=input.value("maxAge",604800000.0);
    return config;
  };
  options.isKilled=[state]{state->trace.push_back("killed");
    return state->killed;};
  SchedulerServices services;
  services.nowMs=[state]{return state->now;};
  services.loadWatcher=[&loop]{Deferred<Unit> done(loop);
    done.resolve({});return done;};
  services.readMissions=[&loop,state](const auto& dir){
    state->trace.push_back("read:"+dir.value_or(""));
    Deferred<std::vector<CronMissionRef>> done(loop);
    done.resolve(parseMissions(state->fileMissions));return done;
  };
  services.removeMissions=[&loop,state](const auto& ids,const auto& dir){
    std::string joined;for(const auto& id:ids){if(!joined.empty())joined+=",";
      joined+=id;}
    state->trace.push_back("remove:"+joined+":"+dir.value_or(""));
    Json remaining=Json::array();
    for(const auto& mission:state->fileMissions){
      bool selected=false;
      for(const auto& id:ids)if(mission.at("id")==id)selected=true;
      if(!selected)remaining.push_back(mission);
    }
    state->fileMissions=std::move(remaining);
    Deferred<Unit> done(loop);done.resolve({});return done;
  };
  services.markFired=[&loop,state](const auto& ids,double now,
    const auto& dir){
    std::string joined;for(const auto& id:ids){if(!joined.empty())joined+=",";
      joined+=id;}
    state->trace.push_back("mark:"+joined+":"+
      batbox::value::numberToString(now)+":"+dir.value_or(""));
    for(auto& mission:state->fileMissions)
      for(const auto& id:ids)if(mission.at("id")==id)
        mission["lastFiredAt"]=now;
    Deferred<Unit> done(loop);done.resolve({});return done;
  };
  services.hasMissionsSync=[state](const auto& dir){
    state->trace.push_back("has:"+dir.value_or(""));
    return !state->fileMissions.empty();
  };
  services.filePath=[](const auto& dir){
    return dir.value_or("/project")+"/.batbox/scheduled_tasks.json";
  };
  services.getFlightMissions=[state]{
    state->trace.push_back("session");
    return parseMissions(state->flightMissions);
  };
  services.removeFlightMissions=[state](const auto& ids){
    std::string joined;for(const auto& id:ids){if(!joined.empty())joined+=",";
      joined+=id;}
    state->trace.push_back("removeSession:"+joined);
    Json remaining=Json::array();
    for(const auto& mission:state->flightMissions){
      bool selected=false;
      for(const auto& id:ids)if(mission.at("id")==id)selected=true;
      if(!selected)remaining.push_back(mission);
    }
    state->flightMissions=std::move(remaining);
  };
  services.getEnabled=[state]{state->trace.push_back("getEnabled");
    return state->enabled;};
  services.setEnabled=[state](bool value){
    state->trace.push_back("setEnabled:"+yes(value));
    state->enabled=value;
  };
  services.acquireLock=[&loop,state](const auto&,const auto&){
    state->trace.push_back("acquire");
    const auto owned=state->lockIndex<state->lockSequence.size()?
      state->lockSequence[state->lockIndex++]:false;
    Deferred<bool> done(loop);done.resolve(owned);return done;
  };
  services.releaseLock=[&loop,state](const auto&,const auto&){
    state->trace.push_back("release");
    Deferred<Unit> done(loop);done.resolve({});return done;
  };
  services.watchFile=[state](const auto& path,auto add,auto change,
    auto unlink){
    state->trace.push_back("watch:"+path+":300");
    auto watcher=std::make_shared<Watcher>(state->trace);
    watcher->add=std::move(add);watcher->change=std::move(change);
    watcher->unlink=std::move(unlink);state->watcher=watcher;
    return std::static_pointer_cast<SchedulerWatcher>(watcher);
  };
  services.interval=[state](double delay,auto callback){
    state->trace.push_back("timer:"+batbox::value::numberToString(delay));
    auto timer=std::make_shared<Timer>(delay,std::move(callback),state->trace);
    state->timers.push_back(timer);
    return std::static_pointer_cast<SchedulerTimer>(timer);
  };
  services.logDebug=[](const std::string&){};
  services.isoTime=[](double ms){return batbox::platform::isoTimestamp(ms);};
  services.cronToHuman=[](const std::string& value){return "human "+value;};
  services.localDateTime=[](double){return "date";};
  const auto scheduler=CronScheduler::create(loop,std::move(options),
    std::move(services));
  Json snapshots=Json::array();
  for(const auto& op:input.at("ops")){
    const auto kind=op.at("kind").get<std::string>();
    if(kind=="start")scheduler->start();
    else if(kind=="stop")scheduler->stop();
    else if(kind=="tick"){
      const auto timers=state->timers;
      for(const auto& timer:timers)
        if(timer->active&&timer->delay==op.at("delay").get<double>())
          timer->callback();
    }else if(kind=="now")state->now=op.at("value").get<double>();
    else if(kind=="enabled")state->enabled=op.at("value").get<bool>();
    else if(kind=="loading")state->loading=op.at("value").get<bool>();
    else if(kind=="killed")state->killed=op.at("value").get<bool>();
    else if(kind=="fileTasks")state->fileMissions=op.at("tasks");
    else if(kind=="sessionTasks")state->flightMissions=op.at("tasks");
    else if(kind=="watch"&&state->watcher){
      const auto event=op.at("event").get<std::string>();
      if(event=="add")state->watcher->add();
      else if(event=="change")state->watcher->change();
      else if(event=="unlink")state->watcher->unlink();
    }
    loop.runReady();
    Json snapshot=Json::object();
    const auto next=scheduler->getNextFireTime();
    snapshot["next"]=next?Json(*next):Json(nullptr);
    snapshot["fileTasks"]=state->fileMissions;
    snapshot["sessionTasks"]=state->flightMissions;
    snapshot["fires"]=state->fires;
    snapshot["missed"]=state->missed;
    snapshot["trace"]=state->trace;
    snapshots.push_back(std::move(snapshot));
  }
  return snapshots;
}
}
int main(){
  for(std::string line;std::getline(std::cin,line);){
    try{std::cout<<batbox::value::stringify(run(batbox::value::parse(line)))<<'\n';}
    catch(const std::exception& error){
      std::cerr<<error.what()<<'\n';return 1;
    }
  }
}
