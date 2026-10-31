#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace batbox::auth::refresh {
struct HttpResponse {
  int status=0;
  std::string statusText;
  Json data=nullptr;
};
struct ProfileServices {
  std::function<event_loop::Deferred<std::optional<Json>>(
    const std::string&)> getRawProfile;
  std::map<std::string,std::string> subscriptionTypes;
};
event_loop::Deferred<Json> fetchProfileInfo(event_loop::Loop& loop,
  const std::string& accessToken,const ProfileServices& services);

struct Services {
  std::function<std::string()> tokenUrl,clientId;
  std::vector<std::string> defaultScopes;
  std::function<event_loop::Deferred<HttpResponse>(
    const std::string&,const Json&,const Json&,double)> httpPost;
  std::function<double()> nowMs;
  std::function<Json()> globalConfig;
  std::function<std::optional<Json>()> existingTokens;
  std::function<event_loop::Deferred<Json>(const std::string&)>
    fetchProfileInfo;
  std::function<void(const std::function<Json(const Json&)>&)>
    saveGlobalConfig;
};
event_loop::Deferred<Json> refreshOAuthToken(event_loop::Loop& loop,
  const std::string& refreshToken,
  const std::optional<std::vector<std::string>>& requestedScopes,
  const Services& services);
}
