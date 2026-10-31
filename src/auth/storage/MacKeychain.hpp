#pragma once
#include "auth/storage/KeychainHelpers.hpp"
#include "auth/storage/Storage.hpp"
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace batbox::auth::storage {
struct SecurityResult {int exitCode=0;std::string stdoutText;};
struct MacKeychainOps {
  std::function<std::string(const std::string&,
    const std::string&)> findSync;
  std::function<event_loop::Deferred<SecurityResult>(
    const std::string&,const std::string&)> findAsync;
  std::function<SecurityResult(const std::vector<std::string>&,
    const std::optional<std::string>&)> executeSync;
  std::function<void(const std::string&,
    const std::string&)> deleteSync;
  std::function<void(const std::string&,const std::string&)> debug;
};
Storage makeMacKeychainStorage(event_loop::Loop& loop,
  std::shared_ptr<KeychainState> state,MacKeychainOps operations);
class KeychainLockDetector {
public:
  KeychainLockDetector(std::function<std::string()> platform,
    std::function<SecurityResult()> showKeychainInfo);
  bool isLocked();
private:
  std::function<std::string()> platform_;
  std::function<SecurityResult()> showKeychainInfo_;
  std::optional<bool> stashed_;
};
}
