#pragma once
#include <functional>
#include <string>

namespace batbox::cron {
struct CronHumanServices {
  std::function<std::string(double,double)> formatLocalTime;
  std::function<std::string(double,double)> formatUtcTimeAsLocal;
  std::function<int(int,double,double)> utcDayOfWeekAsLocal;
};
std::string cronToHuman(const std::string& expression,bool utc,
  const CronHumanServices& services);
CronHumanServices indigenousCronHumanServices();
std::string indigenousLocalDateTime(double epochMs);
}
