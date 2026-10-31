#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::auth::oauth_client {
struct ProfileAccountServices {
  std::function<Json()> getGlobalConfig;
  std::function<void(const std::function<Json(const Json&)>&)>
    saveGlobalConfig;
  std::function<Json(const std::string&)> environment;
  std::function<std::optional<Json>()> getOAuthTokens;
  std::function<bool()> hasProfileScope,isSubscriber;
  std::function<event_loop::Deferred<event_loop::Unit>()> checkAndRefresh;
  std::function<event_loop::Deferred<std::optional<Json>>(
    const std::string&)> getRawProfile;
  std::function<void(const std::string&)> logForDebugging;
  std::string accountUuidEnv="BATBOX_ACCOUNT_UUID";
  std::string userEmailEnv="BATBOX_USER_EMAIL";
  std::string organizationUuidEnv="BATBOX_ORGANIZATION_UUID";
};
void storeOAuthAccountInfo(const Json& fields,
  const ProfileAccountServices& services);
event_loop::Deferred<std::optional<std::string>> getOrganizationUUID(
  event_loop::Loop& loop,const ProfileAccountServices& services);
event_loop::Deferred<bool> populateOAuthAccountInfoIfNeeded(
  event_loop::Loop& loop,const ProfileAccountServices& services);
}
