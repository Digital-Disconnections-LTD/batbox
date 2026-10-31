#pragma once
#include "cron/CronJitter.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <string>

namespace batbox::cron::config {
inline constexpr double jitterRefreshMs=60000;
inline constexpr double gateRefreshMs=300000;
inline constexpr double maxAgeDays=
  defaultCronJitterConfig.recurringMaxAgeMs/86400000;
struct Services {
  // The caller supplies local configuration using Batbox product keys.
  std::function<Json(const std::string&,const Json&,double)>
    getStashedConfiguration;
  std::function<bool()> batTriggersBuilt;
  std::function<bool(const std::string&)> isEnvironmentTruthy;
  std::string enabledKey="batbox_cron_enabled";
  std::string durableKey="batbox_cron_durable";
  std::string jitterKey="batbox_cron_config";
  std::string disableEnvironment="BATBOX_DISABLE_CRON";
};
CronJitterConfig getCronJitterConfig(const Services& services);
bool isCronEnabled(const Services& services);
bool isDurableCronEnabled(const Services& services);
Services indigenousCronConfigurationServices(
  std::function<Json(const std::string&,const Json&,double)>
    getStashedConfiguration,bool batTriggersBuilt);
}
