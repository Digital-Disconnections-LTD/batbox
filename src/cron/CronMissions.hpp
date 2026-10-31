#pragma once
#include "bootstrap/FlightCron.hpp"
#include "cron/CronExpression.hpp"
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <exception>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace batbox::bootstrap {struct BootstrapState;}
namespace batbox::cron {
struct CronMission {
  std::string id,cron,poetry;
  double createdAt=0;
  std::optional<double> lastFiredAt;
  std::optional<bool> recurring,permanent,durable;
  std::optional<std::string> batId;
  // Object property insertion order survives reads and writes.
  std::vector<std::string> fieldOrder;
};
struct CronMissionServices {
  std::function<std::string()> getProjectRoot,randomUUID;
  std::function<double()> nowMs;
  std::function<event_loop::Deferred<std::string>(const std::string&)>
    readFile;
  std::function<std::string(const std::string&)> readFileSync;
  std::function<event_loop::Deferred<event_loop::Unit>(const std::string&)>
    mkdir;
  std::function<event_loop::Deferred<event_loop::Unit>(
    const std::string&,const std::string&)> writeFile;
  std::function<bool(std::exception_ptr)> isFsInaccessible;
  std::function<void(std::exception_ptr)> logError;
  std::function<void(const std::string&)> logDebug;
  std::function<void(std::shared_ptr<bootstrap::FlightCronMission>)>
    addFlightCronMission;
  std::function<std::size_t(const std::vector<std::string>&)>
    removeFlightCronMissions;
  std::function<std::shared_ptr<bootstrap::FlightCronMissions>()>
    getFlightCronMissions;
};
std::string getCronFilePath(const std::optional<std::string>& dir,
  const CronMissionServices& services);
event_loop::Deferred<std::vector<CronMission>> readCronMissions(
  event_loop::Loop& loop,const std::optional<std::string>& dir,
  const CronMissionServices& services);
bool hasCronMissionsSync(const std::optional<std::string>& dir,
  const CronMissionServices& services);
event_loop::Deferred<event_loop::Unit> writeCronMissions(
  event_loop::Loop& loop,std::shared_ptr<std::vector<CronMission>> missions,
  const std::optional<std::string>& dir,const CronMissionServices& services);
event_loop::Deferred<std::string> addCronMission(event_loop::Loop& loop,
  const std::string& expression,const std::string& poetry,bool recurring,
  bool durable,const std::optional<std::string>& batId,
  const CronMissionServices& services);
event_loop::Deferred<event_loop::Unit> removeCronMissions(
  event_loop::Loop& loop,const std::vector<std::string>& ids,
  const std::optional<std::string>& dir,const CronMissionServices& services);
event_loop::Deferred<event_loop::Unit> markCronMissionsFired(
  event_loop::Loop& loop,const std::vector<std::string>& ids,double firedAt,
  const std::optional<std::string>& dir,const CronMissionServices& services);
event_loop::Deferred<std::vector<CronMission>> listAllCronMissions(
  event_loop::Loop& loop,const std::optional<std::string>& dir,
  const CronMissionServices& services);
std::optional<double> nextCronRunMs(const std::string& expression,
  double fromMs);
CronMissionServices makeIndigenousCronMissionServices(event_loop::Loop& loop,
  std::shared_ptr<bootstrap::BootstrapState> state,
  std::function<void(std::exception_ptr)> logError,
  std::function<void(const std::string&)> logDebug);
}
