#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace batbox::auth::helper {
struct CommandResult {
  bool failed=false,timedOut=false;
  int exitCode=0;
  std::string stdoutText,stderrText;
};
struct Services {
  std::function<Json()> configuredHelper;
  std::function<Json(const std::string&)> settingsForSource;
  std::function<Json(const std::string&)> environment;
  std::function<double()> nowMs;
  std::function<bool()> trustAccepted;
  std::function<void(const std::string&,const std::string&)> trustError;
  std::function<void(const std::string&)> consoleError;
  std::function<void(const std::string&,const std::string&)> debug;
  std::function<event_loop::Deferred<CommandResult>(
    const std::string&,double)> executeShell;
  std::string ttlEnvironment="BATBOX_API_KEY_HELPER_TTL_MS";
  std::string feedbackChannel;
};
class Stash {
public:
  Stash(event_loop::Loop& loop,Services services);
  bool isHelperFromProjectOrLocalSettings()const;
  double calculateTTL()const;
  double elapsedMs()const;
  event_loop::Deferred<std::optional<std::string>>
    get(bool isNonInteractiveFlight);
  std::optional<std::string> getStashed()const;
  void clear();
  void prefetchIfSafe(bool isNonInteractiveFlight);
private:
  struct Stashed {std::string value;double timestamp;};
  struct Inflight {
    event_loop::Deferred<std::optional<std::string>> promise;
    std::optional<double> startedAt;
  };
  event_loop::Loop& loop_;
  Services services_;
  std::optional<Stashed> stashed_;
  std::optional<Inflight> inflight_;
  std::uint64_t epoch_=0;
  event_loop::Deferred<std::optional<std::string>> runAndStash(
    bool isNonInteractiveFlight,bool isCold,std::uint64_t epoch);
  event_loop::Deferred<std::optional<std::string>> executeHelper(
    bool isNonInteractiveFlight);
};
}
