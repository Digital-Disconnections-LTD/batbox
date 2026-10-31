#pragma once
#include "cron/CronMissions.hpp"
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace batbox::cron {
struct CronJitterConfig {
  double recurringFrac=0.1;
  double recurringCapMs=15*60*1000;
  double oneShotMaxMs=90*1000;
  double oneShotFloorMs=0;
  double oneShotMinuteMod=30;
  double recurringMaxAgeMs=7*24*60*60*1000;
};
inline constexpr CronJitterConfig defaultCronJitterConfig{};
double jitterFrac(const std::string& missionId);
std::optional<double> jitteredNextCronRunMs(const std::string& expression,
  double fromMs,const std::string& missionId,
  const CronJitterConfig& config=defaultCronJitterConfig);
std::optional<double> oneShotJitteredNextCronRunMs(
  const std::string& expression,double fromMs,const std::string& missionId,
  const CronJitterConfig& config=defaultCronJitterConfig);
using CronMissionRef=std::shared_ptr<CronMission>;
std::vector<CronMissionRef> findMissedMissions(
  const std::vector<CronMissionRef>& missions,double nowMs);
}
