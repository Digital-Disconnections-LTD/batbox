#pragma once
#include "settings/remote/Checksum.hpp"
#include "settings/remote/FetchTypes.hpp"
#include "settings/remote/Security.hpp"
#include "settings/remote/SyncStash.hpp"

namespace batbox::settings::remote {
struct FetchAndLoadServices {
  SyncStash* stash=nullptr;
  std::function<bool()> isEligible;
  std::function<event_loop::Deferred<FetchResult>(
    const std::optional<std::string>&)> fetchWithRetry;
  std::function<event_loop::Deferred<SecurityDecision>(
    const std::optional<Json>&,const Json&)> checkSecurity;
  std::function<bool(SecurityDecision)> handleSecurity;
  std::function<event_loop::Deferred<event_loop::Unit>(const Json&)>
    saveSettings;
  std::function<event_loop::Deferred<event_loop::Unit>(
    const std::string&)> unlinkSettings;
  std::function<void(const std::string&)> debug;
};
event_loop::Deferred<std::optional<Json>> fetchAndLoadRemoteManagedSettings(
  event_loop::Loop& loop,const FetchAndLoadServices& services);
}
