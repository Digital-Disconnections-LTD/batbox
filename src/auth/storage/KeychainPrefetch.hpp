#pragma once
#include "auth/storage/KeychainHelpers.hpp"
#include "event_loop/Deferred.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace batbox::auth::storage {
struct PrefetchResult {
  std::optional<std::string> stdoutText;
  bool timedOut=false;
};
struct PrefetchServices {
  std::function<std::string()> platform;
  std::function<bool()> bareMode;
  std::function<event_loop::Deferred<PrefetchResult>(
    const std::string&)> spawnSecurity;
  std::function<void(const std::optional<std::string>&)> primeStash;
};
class KeychainPrefetch {
public:
  KeychainPrefetch(event_loop::Loop& loop,
    std::shared_ptr<KeychainState> state,PrefetchServices services);
  void start();
  event_loop::Deferred<event_loop::Unit> ensureCompleted();
  std::optional<std::optional<std::string>> legacyApiKeyResult()const;
  void clearLegacyApiKeyResult();
private:
  event_loop::Loop& loop_;
  std::shared_ptr<KeychainState> state_;
  PrefetchServices services_;
  std::optional<event_loop::Deferred<event_loop::Unit>> pending_;
  std::optional<std::optional<std::string>> legacy_;
};
}
