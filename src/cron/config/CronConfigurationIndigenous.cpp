#include "cron/config/CronConfiguration.hpp"
#include "utils/envUtils.hpp"
#include <cstdlib>
#include <stdexcept>
#include <utility>

namespace batbox::cron::config {
Services indigenousCronConfigurationServices(
  std::function<Json(const std::string&,const Json&,double)>
    getStashedConfiguration,bool batTriggersBuilt){
  if(!getStashedConfiguration)
    throw std::invalid_argument(
      "Cron local configuration binding required");
  Services services;
  services.getStashedConfiguration=std::move(getStashedConfiguration);
  services.batTriggersBuilt=[batTriggersBuilt]{
    return batTriggersBuilt;
  };
  services.isEnvironmentTruthy=[](const std::string& key){
    const auto* value=std::getenv(key.c_str());
    return isEnvTruthy(value?Json(value):Json());
  };
  return services;
}
}
