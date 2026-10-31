#include <cstdlib>
#include "cron/CronLock.hpp"
#include "bootstrap/BootstrapState.hpp"
#include "cleanup/Roster.hpp"
#include "runtime/JsonValue.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <unistd.h>

using batbox::Json;
using namespace batbox;
namespace {
std::string category(const std::string& message){
  if(message.find("acquired scheduler lock")!=std::string::npos)
    return "acquired";
  if(message.find("held by session")!=std::string::npos)return "held";
  if(message.find("recovering stale")!=std::string::npos)
    return "recovering";
  if(message.find("released scheduler lock")!=std::string::npos)
    return "released";
  return message;
}
Json run(const Json& test){
  char pattern[]="/tmp/batbox-cron-lock-XXXXXX";
  const auto root=::mkdtemp(pattern);
  if(!root)throw std::runtime_error("mkdtemp failed");
  const std::string dir=root;
  event_loop::Loop loop;
  cleanup::Roster roster(loop);
  auto state=std::make_shared<bootstrap::BootstrapState>();
  state->identity.flightId="mine";
  Json logs=Json::array();
  auto lock=std::make_shared<cron::CronLock>(loop,state,roster,
    [&logs](const std::string& line){logs.push_back(category(line));});
  const cron::SchedulerLockOptions options{dir,std::string("mine")};
  const auto path=lock->getLockPath(options);
  if(test.value("dirExists",true)||test.contains("initial")||
      test.contains("initialRaw"))
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
  if(test.contains("initial")||test.contains("initialRaw")){
    std::ofstream output(path,std::ios::binary);
    if(test.contains("initialRaw"))
      output<<test.at("initialRaw").get<std::string>();
    else{
      const auto initial=test.at("initial");
      const auto kind=initial.at("pid").get<std::string>();
      const auto pid=kind=="self"||kind=="live"?
        static_cast<double>(::getpid()):999999.0;
      Json body=Json::object();
      body["sessionId"]=initial.at("sessionId");
      body["pid"]=pid;body["acquiredAt"]=10;
      output<<value::stringify(body);
    }
  }
  Json snapshots=Json::array();
  for(const auto& op:test.at("ops")){
    Json result=nullptr;
    if(op=="acquire"){
      auto pending=lock->tryAcquire(options);
      pending.then([&result](const bool* value,
        const std::exception_ptr& error){
        if(error)std::rethrow_exception(error);
        result=*value;
      });
      loop.run();
    }else if(op=="release"){
      auto pending=lock->release(options);
      pending.then([](const event_loop::Unit*,
        const std::exception_ptr& error){
        if(error)std::rethrow_exception(error);
      });
      loop.run();
    }
    Json existing=nullptr;
    if(std::filesystem::exists(path)){
      std::ifstream input(path,std::ios::binary);
      const std::string raw((std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
      const auto parsed=value::parse(raw,false);
      if(parsed.is_object()&&parsed.contains("sessionId")&&
         parsed.contains("pid")){
        existing=Json::object();
        existing["sessionId"]=parsed.at("sessionId");
        const auto pid=parsed.at("pid").get<double>();
        existing["pid"]=pid==999999?"stale":
          parsed.at("sessionId")=="mine"?"self":"live";
      }
    }
    Json snapshot=Json::object();snapshot["result"]=result;
    snapshot["lock"]=existing;snapshot["logs"]=logs;
    snapshot["cleanupCount"]=roster.size();
    snapshots.push_back(std::move(snapshot));
  }
  std::filesystem::remove_all(dir);
  return snapshots;
}
}
int main(){
  for(std::string line;std::getline(std::cin,line);){
    try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
    catch(const std::exception& error){
      std::cerr<<error.what()<<'\n';return 1;
    }
  }
}
