#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace batbox::instructions::hooks::runtime {
struct AsyncHookProcess {
  std::function<event_loop::Deferred<std::string>()> getStdout;
  std::function<std::string()> getStderr,status;
  std::function<event_loop::Deferred<int>()> resultCode;
  std::function<void()> cleanup,kill;
};
struct AsyncHookResponseEvent {
  std::string hookId,hookName,hookEvent,output,stdoutText,stderrText,outcome;
  int exitCode=0;
};
struct AsyncHookServices {
  std::function<double()> nowMillis;
  std::function<void(const std::string&,const std::string&)> logDebug;
  std::function<std::function<void()>(const std::string&,const std::string&,
    const std::string&,std::function<event_loop::Deferred<Json>()>)>
    startProgressInterval;
  std::function<void(const AsyncHookResponseEvent&)> emitHookResponse;
  std::function<void()> invalidateFlightEnvStash;
  std::function<std::string(std::exception_ptr)> stringifyThrown;
};
struct PendingAsyncHook {
  std::string processId,hookId,hookName,hookEvent,command;
  std::optional<std::string> gadgetName,pluginId;
  double startTime=0,timeout=0;
  bool responseAttachmentSent=false;
  std::shared_ptr<AsyncHookProcess> process;
  std::function<void()> stopProgressInterval;
};
struct AsyncHookRegistration {
  std::string processId,hookId,hookName,hookEvent,command;
  Json asyncResponse;
  std::optional<std::string> gadgetName,pluginId;
  std::shared_ptr<AsyncHookProcess> process;
};
struct CheckedAsyncHookResponse {
  std::string processId,hookName,hookEvent,stdoutText,stderrText;
  std::optional<std::string> gadgetName,pluginId;
  Json response=Json::object();
  int exitCode=0;
};
class AsyncHookRoster {
public:
  AsyncHookRoster(event_loop::Loop& loop,AsyncHookServices services);
  void registerPendingAsyncHook(const AsyncHookRegistration& registration);
  std::vector<PendingAsyncHook> getPendingAsyncHooks()const;
  event_loop::Deferred<std::vector<CheckedAsyncHookResponse>>
    checkForAsyncHookResponses();
  void removeDeliveredAsyncHooks(const std::vector<std::string>& processIds);
  event_loop::Deferred<event_loop::Unit> finalizePendingAsyncHooks();
  void clearAllAsyncHooks();
  std::size_t size()const;
private:
  friend struct RosterPoll;
  friend struct RosterFinalize;
  event_loop::Loop& loop_;
  AsyncHookServices services_;
  std::vector<std::string> order_;
  std::map<std::string,std::shared_ptr<PendingAsyncHook>> pending_;
  void log(const std::string& message,const std::string& level="info")const;
  void erase(const std::string& processId);
  event_loop::Deferred<event_loop::Unit> finalizeHook(
    const PendingAsyncHook& hook,int exitCode,const std::string& outcome);
};
} // namespace batbox::instructions::hooks::runtime
