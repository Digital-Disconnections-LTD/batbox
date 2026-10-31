#include <cstdlib>
#include "cron/CronSchedulerIndigenous.hpp"
#include "cron/CronLock.hpp"
#include "bootstrap/BootstrapState.hpp"
#include "cleanup/Roster.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/Platform.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <thread>
#include <unistd.h>

using namespace batbox;
namespace {
void writeMissions(const std::string& path,const Json& missions){
  Json body=Json::object();body["tasks"]=missions;
  std::ofstream out(path,std::ios::binary|std::ios::trunc);
  if(!out)throw std::runtime_error("Cannot write cron fixture");
  out<<value::stringify(body,2)<<'\n';
}
template<class Condition>void until(event_loop::Loop& loop,
  Condition condition,double limitMs=3000){
  const auto end=std::chrono::steady_clock::now()+
    std::chrono::milliseconds(static_cast<int>(limitMs));
  while(std::chrono::steady_clock::now()<end){
    loop.runReady();
    if(condition())return;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  throw std::runtime_error("Cron native fixture timed out");
}
Json mission(const std::string& id,double created,bool recurring){
  Json value=Json::object();
  value["id"]=id;value["cron"]="* * * * *";
  value["prompt"]="prompt-"+id;value["createdAt"]=created;
  if(recurring)value["recurring"]=true;
  return value;
}
}
int main(){
  char pattern[]="/tmp/batbox-cron-native-XXXXXX";
  const auto root=::mkdtemp(pattern);
  if(!root)throw std::runtime_error("mkdtemp failed");
  const std::string dir=root;
  const auto folder=std::filesystem::path(dir)/".batbox";
  std::filesystem::create_directories(folder);
  const auto file=(folder/"scheduled_tasks.json").string();
  const auto lockPath=(folder/"scheduled_tasks.lock").string();
  const auto now=static_cast<double>(platform::nowMillis());
  writeMissions(file,Json::array({mission("missed",now-180000,false)}));
  event_loop::Loop loop;
  cleanup::Roster cleanup(loop);
  auto state=std::make_shared<bootstrap::BootstrapState>();
  state->identity.directories.projectRoot=dir;
  state->identity.flightId="owner";
  state->flags.scheduledMissionsEnabled=true;
  std::vector<std::string> logs,fires,missed;
  auto missionServices=cron::makeIndigenousCronMissionServices(loop,state,
    [&logs](std::exception_ptr error){
      try{std::rethrow_exception(error);}
      catch(const std::exception& e){logs.push_back(e.what());}
    },[&logs](const std::string& line){logs.push_back(line);});
  auto services=cron::makeIndigenousSchedulerServices(loop,state,cleanup,
    std::move(missionServices));
  cron::SchedulerOptions options;
  options.onFire=[&fires](const std::string& poetry){fires.push_back(poetry);};
  options.isLoading=[]{return false;};
  options.onMissed=[&missed](const auto& missions){
    for(const auto& value:missions)missed.push_back(value->id);
  };
  auto scheduler=cron::CronScheduler::create(loop,options,services);
  scheduler->start();
  until(loop,[&]{return missed.size()==1&&
    std::filesystem::exists(lockPath);});
  assert(missed==std::vector<std::string>{"missed"});
  until(loop,[&]{
    std::ifstream in(file);const std::string content(
      (std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());
    const auto parsed=value::parse(content,false);
    return parsed.is_object()&&parsed.contains("tasks")&&
      parsed["tasks"].is_array()&&parsed["tasks"].empty();
  });
  auto flight=std::make_shared<bootstrap::FlightCronMission>();
  flight->id="session";flight->cron="* * * * *";
  flight->poetry="session prompt";flight->createdAt=now-180000;
  bootstrap::addFlightCronMission(state->cron,flight);
  scheduler->check();loop.runReady();
  assert(fires==std::vector<std::string>{"session prompt"});
  assert(bootstrap::getFlightCronMissions(state->cron)->empty());
  writeMissions(file,Json::array({mission("11111111",now,true)}));
  until(loop,[&]{scheduler->check();
    return scheduler->getNextFireTime().has_value();});
  assert(*scheduler->getNextFireTime()>now);
  std::filesystem::remove(file);
  until(loop,[&]{return !scheduler->getNextFireTime();});
  writeMissions(file,Json::array({mission("22222222",now,true)}));
  until(loop,[&]{scheduler->check();
    return scheduler->getNextFireTime().has_value();});
  auto contender=std::make_shared<cron::CronLock>(loop,state,cleanup,
    [&logs](const std::string& line){logs.push_back(line);});
  const cron::SchedulerLockOptions contenderOptions{dir,std::string("contender")};
  std::optional<bool> owned;
  contender->tryAcquire(contenderOptions).then([&owned](const bool* value,
    const std::exception_ptr& error){
    if(error)std::rethrow_exception(error);
    owned=*value;
  });
  until(loop,[&]{return owned.has_value();});
  assert(!*owned);
  scheduler->stop();
  until(loop,[&]{return !std::filesystem::exists(lockPath);});
  owned.reset();
  contender->tryAcquire(contenderOptions).then([&owned](const bool* value,
    const std::exception_ptr& error){
    if(error)std::rethrow_exception(error);
    owned=*value;
  });
  until(loop,[&]{return owned.has_value();});
  assert(*owned);
  contender->release(contenderOptions);
  until(loop,[&]{return !std::filesystem::exists(lockPath);});
  std::filesystem::remove_all(dir);
  std::cout<<"native cron file/watch/lock lifecycle passed\n";
}
