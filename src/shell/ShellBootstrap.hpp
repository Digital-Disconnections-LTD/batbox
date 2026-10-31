#pragma once
#include "event_loop/Deferred.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::shell {
struct DiscoveryServices {
  std::optional<std::string> shellOverride;
  std::optional<std::string> preferredShell;
  std::function<bool(const std::string&)> isExecutable;
  std::function<event_loop::Deferred<std::optional<std::string>>(const std::string&)> which;
  std::function<void(const std::string&)> debug;
  std::function<void(const std::string&)> error;
};
// Select a suitable shell using Batbox configuration and environment settings.
event_loop::Deferred<std::string> findSuitableShellAsync(event_loop::Loop& loop,
  const DiscoveryServices& services);
// Indigenous process bindings for executable lookup and availability checks.
// The host passes its environment map to indigenousWhich.
bool indigenousIsExecutable(const std::string& shellPath);
std::function<event_loop::Deferred<std::optional<std::string>>(const std::string&)>
indigenousWhich(event_loop::Loop& loop,const std::string& pathEnvironment);
} // namespace batbox::shell
