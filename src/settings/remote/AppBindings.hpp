#pragma once
#include "cleanup/Roster.hpp"
#include "settings/Loader.hpp"
#include "settings/persistence/ChangeEvents.hpp"
#include "settings/remote/FetchOne.hpp"
#include "settings/remote/Lifecycle.hpp"
#include "settings/remote/Retry.hpp"
#include "settings/schema/IndigenousBindings.hpp"

namespace batbox::settings::remote {
FetchOneServices bindIndigenousFetchOne(event_loop::Loop& loop,
  AuthServices auth,
  std::function<event_loop::Deferred<event_loop::Unit>()> refreshOAuth,
  std::function<std::string()> baseApiUrl,
  std::function<std::string()> userBat,
  const schema::MainConfig& schemaConfig,
  const PermissionRuleServices& permissionRules,
  std::function<void(const std::string&)> debug,
  std::string settingsPath="/api/batbox/settings");
RetryServices bindIndigenousRetry(event_loop::Loop& loop,
  FetchOneServices fetchOne,
  std::function<double(int)> getRetryDelay,
  std::function<void(const std::string&)> debug);
FetchAndLoadServices bindIndigenousFetchAndLoad(event_loop::Loop& loop,
  SyncStash& stash,Eligibility& eligibility,RetryServices retry,
  SecurityServices security,
  std::function<void(const std::string&)> debug);
LifecycleServices bindIndigenousLifecycle(event_loop::Loop& loop,
  SyncStash& stash,Eligibility& eligibility,
  FetchAndLoadServices fetchAndLoad,
  persistence::ChangeEvents& changes,
  cleanup::Roster& roster,
  std::function<void(const std::string&)> debug);
void bindRemoteToLoader(SyncStash& stash,LoadServices& loader);
}
