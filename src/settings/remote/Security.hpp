#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace batbox::settings::remote {
struct DangerousConfig {
  std::vector<std::string> shellSettings;
  std::unordered_set<std::string> safeEnvVars;
};
DangerousConfig defaultDangerousConfig();
struct DangerousSettings {
  Json shellSettings=Json::object(),envVars=Json::object();
  bool hasHooks=false;
  std::optional<Json> hooks;
};
DangerousSettings extractDangerousSettings(const std::optional<Json>& settings,
  const DangerousConfig& config);
bool hasDangerousSettings(const DangerousSettings& dangerous);
bool hasDangerousSettingsChanged(const std::optional<Json>& oldSettings,
  const std::optional<Json>& newSettings,const DangerousConfig& config);
std::vector<std::string> formatDangerousSettingsList(
  const DangerousSettings& dangerous);

enum class SecurityDecision {Approved,Rejected,NoCheckNeeded};
struct SecurityServices {
  DangerousConfig config;
  std::function<bool()> isInteractive;
  std::function<event_loop::Deferred<SecurityDecision>(const Json&)> showDialog;
  std::function<void(int)> gracefulShutdown;
};
event_loop::Deferred<SecurityDecision> checkManagedSettingsSecurity(
  event_loop::Loop& loop,const std::optional<Json>& stashedSettings,
  const std::optional<Json>& newSettings,const SecurityServices& services);
bool handleSecurityCheckResult(SecurityDecision result,
  const SecurityServices& services);
}
