#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace batbox::auth::storage {
struct KeychainServices {
  std::function<std::string()> configHome;
  std::function<std::optional<std::string>()> configDirOverride,userEnv;
  std::function<std::string()> osUsername;
  std::function<double()> nowMs;
  std::function<std::string()> oauthFileSuffix;
  std::string credentialServiceSuffix="-credentials";
};
class KeychainState {
public:
  static constexpr double stashTtlMs=30000;
  explicit KeychainState(KeychainServices services);
  std::string serviceName(const std::string& suffix={})const;
  std::string username()const;
  void clear();
  void prime(const std::optional<std::string>& stdoutText);
  const KeychainServices& services()const{return services_;}
  std::optional<Json> data;
  double stashedAt=0;
  std::uint64_t generation=0;
  std::optional<event_loop::Deferred<std::optional<Json>>> readInFlight;
private:
  KeychainServices services_;
};
}
