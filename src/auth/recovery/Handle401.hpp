#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <map>
#include <optional>
#include <string>

namespace batbox::auth::recovery {
struct Services {
  std::function<void()> clearOAuthTokenStash;
  std::function<event_loop::Deferred<std::optional<Json>>()> readTokensAsync;
  std::function<event_loop::Deferred<bool>(int,bool)> checkAndRefresh;
};
class Handle401 {
public:
  Handle401(event_loop::Loop& loop,Services services);
  event_loop::Deferred<bool> handle(const std::string& failedAccessToken);
private:
  event_loop::Loop& loop_;
  Services services_;
  std::map<std::string,event_loop::Deferred<bool>> pending_;
};
}
