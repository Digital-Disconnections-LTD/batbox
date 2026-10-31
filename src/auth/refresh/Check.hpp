#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace batbox::auth::refresh {
using ReleaseLock=std::function<event_loop::Deferred<event_loop::Unit>()>;
struct LockError:std::runtime_error {
  std::string code;
  LockError(std::string name,std::string message):
    std::runtime_error(message),code(std::move(name)){}
};
struct CheckServices {
  std::function<event_loop::Deferred<event_loop::Unit>()> invalidateDiskStash;
  std::function<std::optional<Json>()> stashedTokens;
  std::function<void()> clearTokenMemo,clearKeychainStash;
  std::function<event_loop::Deferred<std::optional<Json>>()> readTokensAsync;
  std::function<double()> nowMs,random;
  std::function<std::string()> configHome;
  std::function<event_loop::Deferred<event_loop::Unit>(
    const std::string&)> mkdirRecursive;
  std::function<event_loop::Deferred<ReleaseLock>(
    const std::string&)> lockDirectory;
  std::function<event_loop::Deferred<event_loop::Unit>(double)> sleep;
  std::function<event_loop::Deferred<Json>(
    const std::string&,
    const std::optional<std::vector<std::string>>&)> refreshToken;
  std::function<void(const Json&)> saveTokens;
  std::function<void(const std::string&)> logError;
  std::string inferenceScope="user:inference";
};
class CheckController {
public:
  CheckController(event_loop::Loop& loop,CheckServices services);
  event_loop::Deferred<bool> check(int retryCount=0,bool force=false);
private:
  event_loop::Loop& loop_;
  CheckServices services_;
  std::optional<event_loop::Deferred<bool>> pending_;
  event_loop::Deferred<bool> attempt(int retryCount,bool force);
};
}
