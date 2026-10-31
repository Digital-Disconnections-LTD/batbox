#pragma once
#include <optional>
#include <string>
#include <vector>

namespace batbox::cron {
struct CronFields {
  std::vector<int> minute,hour,dayOfMonth,month,dayOfWeek;
};
std::optional<CronFields> parseCronExpression(const std::string& expression);
std::optional<double> computeNextCronRun(const CronFields& fields,
  double fromMs);
}
