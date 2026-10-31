#pragma once
#include "auth/refresh/Refresh.hpp"

namespace batbox::auth::refresh {
struct ProfileHttpServices {
  std::function<std::string()> baseApiUrl;
  std::function<event_loop::Deferred<HttpResponse>(
    const std::string&,const Json&,double)> httpGet;
  std::function<void(const std::string&)> logError;
  std::string profilePath="/api/oauth/profile";
};
event_loop::Deferred<std::optional<Json>> getRawOAuthProfile(
  event_loop::Loop& loop,const std::string& accessToken,
  const ProfileHttpServices& services);
}
