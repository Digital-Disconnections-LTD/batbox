#pragma once
#include "event_loop/Deferred.hpp"
#include <functional>
#include <string>

namespace batbox::auth::refresh {
struct InvalidateServices {
  std::function<std::string()> configHome;
  std::function<event_loop::Deferred<double>(const std::string&)> statMtimeMs;
  std::function<void()> clearFullStash,clearMemoOnly;
  std::string credentialsFile=".batbox-credentials.json";
};
class DiskInvalidation {
public:
  DiskInvalidation(event_loop::Loop& loop,InvalidateServices services);
  event_loop::Deferred<event_loop::Unit> invalidateIfDiskChanged();
private:
  event_loop::Loop& loop_;
  InvalidateServices services_;
  double lastMtimeMs_=0;
};
event_loop::Deferred<double> indigenousStatMtimeMs(event_loop::Loop& loop,
  const std::string& path);
}
