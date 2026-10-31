#pragma once
#include <functional>
#include <string>

namespace batbox::cron::use_scheduled {
std::string formatCronFireTime(double epochMs);
std::function<std::string()> indigenousCronFireTimeFormatter();
}
