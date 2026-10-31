#pragma once
#include <functional>
#include <optional>
#include <string>

namespace batbox::cron::locale {
std::string formatLocalDateTime(double epochMs,
  const std::optional<std::string>& locale={},
  const std::optional<std::string>& timeZone={});
std::function<std::string(double)> indigenousNotificationDateFormatter();
}
