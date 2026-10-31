#include "cron/CronMissions.hpp"
#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <utility>

namespace batbox::cron {
namespace {
Json missionToJson(const CronMission& mission,bool stripDurable){
  Json out=Json::object();
  const auto write=[&](const std::string& name){
    if(name=="id")out[name]=mission.id;
    else if(name=="cron")out[name]=mission.cron;
    else if(name=="prompt")out[name]=mission.poetry;
    else if(name=="createdAt")out[name]=mission.createdAt;
    else if(name=="lastFiredAt"&&mission.lastFiredAt)
      out[name]=*mission.lastFiredAt;
    else if(name=="recurring"&&mission.recurring)
      out[name]=*mission.recurring;
    else if(name=="permanent"&&mission.permanent)
      out[name]=*mission.permanent;
    else if(name=="durable"&&!stripDurable&&mission.durable)
      out[name]=*mission.durable;
    else if(name=="agentId"&&mission.batId)
      out[name]=*mission.batId;
  };
  for(const auto& name:mission.fieldOrder)write(name);
  for(const auto& name:{"id","cron","prompt","createdAt",
      "lastFiredAt","recurring","permanent","durable","agentId"})
    if(!out.contains(name))write(name);
  return out;
}
event_loop::Deferred<event_loop::Unit> settled(event_loop::Loop& loop){
  event_loop::Deferred<event_loop::Unit> output(loop);
  output.resolve({});return output;
}
}
event_loop::Deferred<event_loop::Unit> writeCronMissions(
  event_loop::Loop& loop,std::shared_ptr<std::vector<CronMission>> missions,
  const std::optional<std::string>& dir,const CronMissionServices& services){
  if(!missions||!services.mkdir||!services.writeFile)
    throw std::invalid_argument("Cron write bindings required");
  const auto root=dir?*dir:(services.getProjectRoot?
    services.getProjectRoot():throw std::invalid_argument(
      "getProjectRoot binding required"));
  event_loop::Deferred<event_loop::Unit> output(loop);
  const auto directory=(std::filesystem::path(root)/".batbox")
    .lexically_normal().string();
  try{
    auto created=services.mkdir(directory);
    created.then([&loop,output,missions,root,services](
      const event_loop::Unit*,const std::exception_ptr& error){
      if(error){output.reject(error);return;}
      try{
        Json body=Json::object();body["tasks"]=Json::array();
        for(const auto& mission:*missions)
          body["tasks"].push_back(missionToJson(mission,true));
        const auto text=value::stringify(body,2)+"\n";
        auto written=services.writeFile(getCronFilePath(root,services),
          text);
        written.then([output](const event_loop::Unit*,
          const std::exception_ptr& failure){
          if(failure)output.reject(failure);
          else output.resolve({});
        });
      }catch(...){output.reject(std::current_exception());}
    });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
event_loop::Deferred<std::string> addCronMission(event_loop::Loop& loop,
  const std::string& expression,const std::string& poetry,bool recurring,
  bool durable,const std::optional<std::string>& batId,
  const CronMissionServices& services){
  if(!services.randomUUID||!services.nowMs)
    throw std::invalid_argument("Cron UUID and clock bindings required");
  event_loop::Deferred<std::string> output(loop);
  try{
    const auto id=value::slice(services.randomUUID(),0,8);
    CronMission mission;
    mission.id=id;mission.cron=expression;mission.poetry=poetry;
    mission.createdAt=services.nowMs();
    mission.fieldOrder={"id","cron","prompt","createdAt"};
    if(recurring){mission.recurring=true;mission.fieldOrder.push_back("recurring");}
    if(!durable){
      if(!services.addFlightCronMission)
        throw std::invalid_argument("Session cron binding required");
      auto flight=std::make_shared<bootstrap::FlightCronMission>();
      flight->id=mission.id;flight->cron=mission.cron;
      flight->poetry=mission.poetry;flight->createdAt=mission.createdAt;
      flight->recurring=mission.recurring;
      if(batId&&!batId->empty())flight->batId=*batId;
      services.addFlightCronMission(std::move(flight));
      output.resolve(id);return output;
    }
    auto read=readCronMissions(loop,std::nullopt,services);
    read.then([&loop,output,mission=std::move(mission),id,services](
      const std::vector<CronMission>* existing,
      const std::exception_ptr& error){
      if(error){output.reject(error);return;}
      try{
        auto missions=std::make_shared<std::vector<CronMission>>(*existing);
        missions->push_back(mission);
        auto written=writeCronMissions(loop,missions,std::nullopt,services);
        written.then([output,id](const event_loop::Unit*,
          const std::exception_ptr& failure){
          if(failure)output.reject(failure);
          else output.resolve(id);
        });
      }catch(...){output.reject(std::current_exception());}
    });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
event_loop::Deferred<event_loop::Unit> removeCronMissions(
  event_loop::Loop& loop,const std::vector<std::string>& ids,
  const std::optional<std::string>& dir,const CronMissionServices& services){
  if(ids.empty())return settled(loop);
  event_loop::Deferred<event_loop::Unit> output(loop);
  try{
    if(!dir){
      if(!services.removeFlightCronMissions)
        throw std::invalid_argument("Session cron removal binding required");
      if(services.removeFlightCronMissions(ids)==ids.size()){
        output.resolve({});return output;
      }
    }
    auto read=readCronMissions(loop,dir,services);
    read.then([&loop,output,ids,dir,services](
      const std::vector<CronMission>* missions,
      const std::exception_ptr& error){
      if(error){output.reject(error);return;}
      std::vector<CronMission> remaining;
      for(const auto& mission:*missions)
        if(std::find(ids.begin(),ids.end(),mission.id)==ids.end())
          remaining.push_back(mission);
      if(remaining.size()==missions->size()){
        output.resolve({});return;
      }
      try{
        auto written=writeCronMissions(loop,
          std::make_shared<std::vector<CronMission>>(std::move(remaining)),
          dir,services);
        written.then([output](const event_loop::Unit*,
          const std::exception_ptr& failure){
          if(failure)output.reject(failure);
          else output.resolve({});
        });
      }catch(...){output.reject(std::current_exception());}
    });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
event_loop::Deferred<event_loop::Unit> markCronMissionsFired(
  event_loop::Loop& loop,const std::vector<std::string>& ids,double firedAt,
  const std::optional<std::string>& dir,const CronMissionServices& services){
  if(ids.empty())return settled(loop);
  event_loop::Deferred<event_loop::Unit> output(loop);
  try{
    auto read=readCronMissions(loop,dir,services);
    read.then([&loop,output,ids,firedAt,dir,services](
      const std::vector<CronMission>* missions,
      const std::exception_ptr& error){
      if(error){output.reject(error);return;}
      auto updated=std::make_shared<std::vector<CronMission>>(*missions);
      bool changed=false;
      for(auto& mission:*updated)
        if(std::find(ids.begin(),ids.end(),mission.id)!=ids.end()){
          if(!mission.lastFiredAt)mission.fieldOrder.push_back("lastFiredAt");
          mission.lastFiredAt=firedAt;changed=true;
        }
      if(!changed){output.resolve({});return;}
      try{
        auto written=writeCronMissions(loop,updated,dir,services);
        written.then([output](const event_loop::Unit*,
          const std::exception_ptr& failure){
          if(failure)output.reject(failure);
          else output.resolve({});
        });
      }catch(...){output.reject(std::current_exception());}
    });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
event_loop::Deferred<std::vector<CronMission>> listAllCronMissions(
  event_loop::Loop& loop,const std::optional<std::string>& dir,
  const CronMissionServices& services){
  event_loop::Deferred<std::vector<CronMission>> output(loop);
  try{
    auto read=readCronMissions(loop,dir,services);
    read.then([output,dir,services](const std::vector<CronMission>* files,
      const std::exception_ptr& error){
      if(error){output.reject(error);return;}
      auto combined=*files;
      if(!dir){
        if(!services.getFlightCronMissions){
          output.reject(std::make_exception_ptr(std::invalid_argument(
            "Session cron list binding required")));return;
        }
        for(const auto& mission:*services.getFlightCronMissions()){
          CronMission copy;
          copy.id=mission->id;copy.cron=mission->cron;
          copy.poetry=mission->poetry;copy.createdAt=mission->createdAt;
          copy.recurring=mission->recurring;copy.batId=mission->batId;
          copy.durable=false;combined.push_back(std::move(copy));
        }
      }
      output.resolve(std::move(combined));
    });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
}
