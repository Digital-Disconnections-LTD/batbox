#include "cron/CronMissions.hpp"
#include <filesystem>
#include <stdexcept>
#include <utility>

namespace batbox::cron {
namespace {
Json parseSafely(const std::string& raw){
  if(raw.empty())return nullptr;
  auto parsed=value::parse(raw,false);
  return parsed.is_discarded()?Json(nullptr):parsed;
}
std::vector<CronMission> parseMissions(const Json& parsed,
  const CronMissionServices& services){
  if(parsed.is_null()||!parsed.is_object()||
     !parsed.contains("tasks")||!parsed["tasks"].is_array())return {};
  std::vector<CronMission> out;
  for(const auto& mission:parsed["tasks"]){
    if(!mission.is_object()||!mission.contains("id")||
       !mission["id"].is_string()||!mission.contains("cron")||
       !mission["cron"].is_string()||!mission.contains("prompt")||
       !mission["prompt"].is_string()||!mission.contains("createdAt")||
       !mission["createdAt"].is_number()){
      services.logDebug("[ScheduledTasks] skipping malformed task: "+
        value::stringify(mission));
      continue;
    }
    const auto expression=mission["cron"].get<std::string>();
    if(!parseCronExpression(expression)){
      services.logDebug("[ScheduledTasks] skipping task "+
        mission["id"].get<std::string>()+" with invalid cron '"+
        expression+"'");
      continue;
    }
    CronMission valid;
    valid.id=mission["id"].get<std::string>();
    valid.cron=expression;
    valid.poetry=mission["prompt"].get<std::string>();
    valid.createdAt=mission["createdAt"].get<double>();
    valid.fieldOrder={"id","cron","prompt","createdAt"};
    if(mission.contains("lastFiredAt")&&mission["lastFiredAt"].is_number())
      {valid.lastFiredAt=mission["lastFiredAt"].get<double>();
       valid.fieldOrder.push_back("lastFiredAt");}
    if(mission.contains("recurring")&&value::truthy(mission["recurring"]))
      {valid.recurring=true;valid.fieldOrder.push_back("recurring");}
    if(mission.contains("permanent")&&value::truthy(mission["permanent"]))
      {valid.permanent=true;valid.fieldOrder.push_back("permanent");}
    out.push_back(std::move(valid));
  }
  return out;
}
}
std::string getCronFilePath(const std::optional<std::string>& dir,
  const CronMissionServices& services){
  if(!dir&&!services.getProjectRoot)
    throw std::invalid_argument("getProjectRoot binding required");
  const auto root=dir?*dir:services.getProjectRoot();
  return (std::filesystem::path(root)/".batbox"/
    "scheduled_tasks.json").lexically_normal().string();
}
event_loop::Deferred<std::vector<CronMission>> readCronMissions(
  event_loop::Loop& loop,const std::optional<std::string>& dir,
  const CronMissionServices& services){
  if(!services.readFile||!services.isFsInaccessible||!services.logError||
     !services.logDebug)
    throw std::invalid_argument("Cron read bindings required");
  event_loop::Deferred<std::vector<CronMission>> output(loop);
  try{
    auto file=services.readFile(getCronFilePath(dir,services));
    file.then([output,services](const std::string* raw,
      const std::exception_ptr& error){
      try{
        if(error){
          if(!services.isFsInaccessible(error))services.logError(error);
          output.resolve({});return;
        }
        output.resolve(parseMissions(parseSafely(*raw),services));
      }catch(...){output.reject(std::current_exception());}
    });
  }catch(...){
    const auto error=std::current_exception();
    try{
      if(!services.isFsInaccessible(error))services.logError(error);
      output.resolve({});
    }catch(...){output.reject(std::current_exception());}
  }
  return output;
}
bool hasCronMissionsSync(const std::optional<std::string>& dir,
  const CronMissionServices& services){
  if(!services.readFileSync)
    throw std::invalid_argument("Cron sync read binding required");
  std::string raw;
  try{raw=services.readFileSync(getCronFilePath(dir,services));}
  catch(...){return false;}
  const auto parsed=parseSafely(raw);
  return parsed.is_object()&&parsed.contains("tasks")&&
    parsed["tasks"].is_array()&&!parsed["tasks"].empty();
}
std::optional<double> nextCronRunMs(const std::string& expression,
  double fromMs){
  const auto fields=parseCronExpression(expression);
  if(!fields)return std::nullopt;
  return computeNextCronRun(*fields,fromMs);
}
}
