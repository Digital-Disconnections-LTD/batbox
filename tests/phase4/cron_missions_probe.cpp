#include "cron/CronMissions.hpp"
#include "runtime/JsonValue.hpp"
#include <cerrno>
#include <iostream>
#include <map>
#include <stdexcept>
#include <system_error>

namespace {
using batbox::Json;
batbox::cron::CronMission readMission(const Json& data){
  batbox::cron::CronMission mission;
  for(auto it=data.begin();it!=data.end();++it)
    mission.fieldOrder.push_back(it.key());
  mission.id=data.at("id").get<std::string>();
  mission.cron=data.at("cron").get<std::string>();
  mission.poetry=data.at("prompt").get<std::string>();
  mission.createdAt=data.at("createdAt").get<double>();
  if(data.contains("lastFiredAt"))
    mission.lastFiredAt=data["lastFiredAt"].get<double>();
  if(data.contains("recurring"))
    mission.recurring=data["recurring"].get<bool>();
  if(data.contains("permanent"))
    mission.permanent=data["permanent"].get<bool>();
  if(data.contains("durable"))
    mission.durable=data["durable"].get<bool>();
  if(data.contains("agentId"))
    mission.batId=data["agentId"].get<std::string>();
  return mission;
}
Json writeMission(const batbox::cron::CronMission& mission){
  Json out={{"id",mission.id},{"cron",mission.cron},
    {"prompt",mission.poetry},{"createdAt",mission.createdAt}};
  if(mission.lastFiredAt)out["lastFiredAt"]=*mission.lastFiredAt;
  if(mission.recurring)out["recurring"]=*mission.recurring;
  if(mission.permanent)out["permanent"]=*mission.permanent;
  if(mission.durable)out["durable"]=*mission.durable;
  if(mission.batId)out["agentId"]=*mission.batId;
  return out;
}
Json writeFlight(const batbox::bootstrap::FlightCronMission& mission){
  Json out={{"id",mission.id},{"cron",mission.cron},
    {"prompt",mission.poetry},{"createdAt",mission.createdAt}};
  if(mission.recurring)out["recurring"]=*mission.recurring;
  if(mission.batId)out["agentId"]=*mission.batId;
  return out;
}
std::optional<std::string> directory(const Json& op){
  if(!op.contains("dir")||op["dir"].is_null())return std::nullopt;
  return op["dir"].get<std::string>();
}
int errnoCode(const std::string& value){
  if(value=="EACCES")return EACCES;
  if(value=="EPERM")return EPERM;
  if(value=="ENOTDIR")return ENOTDIR;
  if(value=="ELOOP")return ELOOP;
  if(value=="EIO")return EIO;
  return ENOENT;
}
template<class T>T wait(batbox::event_loop::Loop& loop,
  batbox::event_loop::Deferred<T> pending){
  std::optional<T> value;std::exception_ptr error;
  pending.then([&](const T* result,const std::exception_ptr& failure){
    if(failure)error=failure;else value=*result;
  });
  loop.run();
  if(error)std::rethrow_exception(error);
  if(!value)throw std::runtime_error("Cron promise did not settle");
  return *value;
}
}
int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    event_loop::Loop loop;
    Json trace=Json::array(),results=Json::array();
    std::map<std::string,std::string> files;
    if(test.contains("files"))for(auto it=test["files"].begin();
      it!=test["files"].end();++it)
      files[it.key()]=it.value().get<std::string>();
    bootstrap::FlightCronState flight;
    if(test.contains("sessionTasks"))for(const auto& value:test["sessionTasks"]){
      auto mission=std::make_shared<bootstrap::FlightCronMission>();
      mission->id=value.at("id").get<std::string>();
      mission->cron=value.at("cron").get<std::string>();
      mission->poetry=value.at("prompt").get<std::string>();
      mission->createdAt=value.at("createdAt").get<double>();
      if(value.contains("recurring"))
        mission->recurring=value["recurring"].get<bool>();
      if(value.contains("agentId"))
        mission->batId=value["agentId"].get<std::string>();
      flight.flightCronMissions->push_back(mission);
    }
    cron::CronMissionServices services;
    services.getProjectRoot=[&]{return test.value("projectRoot",
      std::string("/project"));};
    services.randomUUID=[&]{trace.push_back("uuid");return
      std::string("abcd1234-1234-4123-8123-123456789abc");};
    services.nowMs=[&]{trace.push_back("now");return
      test.value("now",42.0);};
    services.readFile=[&](const std::string& path){
      event_loop::Deferred<std::string> output(loop);
      trace.push_back("read:"+path);
      const auto error=test.value("readError",std::string());
      if(!error.empty()||!files.contains(path))
        output.reject(std::make_exception_ptr(std::system_error(
          errnoCode(error),std::generic_category(),"read")));
      else output.resolve(files.at(path));
      return output;
    };
    services.readFileSync=[&](const std::string& path){
      trace.push_back("readSync:"+path);
      const auto error=test.value("readError",std::string());
      if(!error.empty()||!files.contains(path))
        throw std::system_error(errnoCode(error),std::generic_category(),
          "readSync");
      return files.at(path);
    };
    services.mkdir=[&](const std::string& path){
      event_loop::Deferred<event_loop::Unit> output(loop);
      trace.push_back("mkdir:"+path);output.resolve({});return output;
    };
    services.writeFile=[&](const std::string& path,const std::string& text){
      event_loop::Deferred<event_loop::Unit> output(loop);
      trace.push_back("write:"+path);files[path]=text;
      output.resolve({});return output;
    };
    services.isFsInaccessible=[](std::exception_ptr error){
      try{if(error)std::rethrow_exception(error);}
      catch(const std::system_error& ex){
        const int code=ex.code().value();
        return code==ENOENT||code==EACCES||code==EPERM||
          code==ENOTDIR||code==ELOOP;
      }catch(...){}
      return false;
    };
    services.logError=[&](std::exception_ptr){trace.push_back("logError");};
    services.logDebug=[&](const std::string& message){
      trace.push_back("debug:"+message);
    };
    services.addFlightCronMission=[&](
      std::shared_ptr<bootstrap::FlightCronMission> mission){
      bootstrap::addFlightCronMission(flight,std::move(mission));
      trace.push_back("addSession");
    };
    services.removeFlightCronMissions=[&](
      const std::vector<std::string>& ids){
      trace.push_back("removeSession");
      return bootstrap::removeFlightCronMissions(flight,ids);
    };
    services.getFlightCronMissions=[&]{
      return bootstrap::getFlightCronMissions(flight);
    };
    if(test.contains("ops"))for(const auto& op:test["ops"]){
      const auto kind=op["kind"].get<std::string>();
      const auto dir=directory(op);
      Json value=nullptr;
      if(kind=="path")value=cron::getCronFilePath(dir,services);
      else if(kind=="read"){
        value=Json::array();
        for(const auto& mission:wait(loop,cron::readCronMissions(loop,dir,services)))
          value.push_back(writeMission(mission));
      }else if(kind=="has")value=cron::hasCronMissionsSync(dir,services);
      else if(kind=="write"){
        auto missions=std::make_shared<std::vector<cron::CronMission>>();
        for(const auto& mission:op["tasks"])missions->push_back(readMission(mission));
        wait(loop,cron::writeCronMissions(loop,missions,dir,services));
      }else if(kind=="add"){
        std::optional<std::string> bat;
        if(op.contains("agentId")&&!op["agentId"].is_null())
          bat=op["agentId"].get<std::string>();
        value=wait(loop,cron::addCronMission(loop,
          op["cron"].get<std::string>(),op["prompt"].get<std::string>(),
          op["recurring"].get<bool>(),op["durable"].get<bool>(),
          bat,services));
      }else if(kind=="remove"){
        std::vector<std::string> ids;
        for(const auto& id:op["ids"])ids.push_back(id.get<std::string>());
        wait(loop,cron::removeCronMissions(loop,ids,dir,services));
      }else if(kind=="mark"){
        std::vector<std::string> ids;
        for(const auto& id:op["ids"])ids.push_back(id.get<std::string>());
        wait(loop,cron::markCronMissionsFired(loop,ids,
          op["firedAt"].get<double>(),dir,services));
      }else if(kind=="list"){
        value=Json::array();
        for(const auto& mission:wait(loop,cron::listAllCronMissions(
          loop,dir,services)))value.push_back(writeMission(mission));
      }else if(kind=="next"){
        const auto next=cron::nextCronRunMs(
          op["cron"].get<std::string>(),op["fromMs"].get<double>());
        if(next)value=*next;
      }else throw std::runtime_error("Unknown cron task operation");
      results.push_back(value);
    }
    Json fileOutput=Json::object();
    for(const auto& [path,content]:files)fileOutput[path]=content;
    Json flightOutput=Json::array();
    for(const auto& mission:*flight.flightCronMissions)
      flightOutput.push_back(writeFlight(*mission));
    std::cout<<value::stringify(Json{{"results",results},{"trace",trace},
      {"files",fileOutput},{"sessionTasks",flightOutput}})<<'\n';
  }
}
