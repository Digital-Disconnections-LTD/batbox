#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::settings::remote {
struct SyncStashServices {
  std::function<std::string()> configHome;
  std::function<std::string(const std::string&)> readFileSync;
  std::function<void()> resetMergedSettingsStash;
};
class SyncStash {
public:
  explicit SyncStash(SyncStashServices services);
  void setFlightStash(std::optional<Json> value);
  void resetSyncStash();
  bool setEligibility(bool value);
  std::string getSettingsPath()const;
  std::optional<Json> getRemoteManagedSettingsSyncFromStash();
  std::optional<bool> eligibility()const{return eligible_;}
private:
  SyncStashServices services_;
  std::optional<Json> flightStash_;
  std::optional<bool> eligible_;
  std::optional<Json> loadSettings();
};
SyncStashServices indigenousSyncStashServices(
  std::function<std::string()> configHome,
  std::function<void()> resetMergedSettingsStash);
}
