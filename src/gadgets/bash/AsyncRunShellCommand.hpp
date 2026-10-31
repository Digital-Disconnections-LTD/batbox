#pragma once
#include "event_loop/Deferred.hpp"
#include "shell/CommandHandle.hpp"
#include "shell/MissionOutputPoller.hpp"
#include "gadgets/bash/RunShellCommand.hpp"
#include <deque>
#include <optional>

namespace batbox::bash_gadget {
struct AsyncShellCommand {
  event_loop::Deferred<shell::ExecResult> result;
  std::string missionId,outputPath;
  std::function<shell::CommandStatus()> status;
  std::function<void()> cleanup;
  std::function<bool(const std::string&)> background;
  std::function<void(const shell::CommandHandle::TimeoutCallback&)> onTimeout;
  std::function<std::optional<shell::CommandHandle::LargeOutputInfo>()> largeOutputInfo;
  explicit AsyncShellCommand(event_loop::Deferred<shell::ExecResult> result):
    result(std::move(result)){}
};
// Production ShellCommand boundary over the managed child and owner result.
AsyncShellCommand bindAsyncShellCommand(event_loop::Loop& loop,shell::CommandHandle handle);
event_loop::Deferred<AsyncShellCommand> execManagedOnLoop(event_loop::Loop& loop,
  std::shared_ptr<shell::ManagedShell> shell,shell::RunOptions options,
  bool expectTimeoutCallback);

struct AsyncExecRequest {
  std::string command;
  double timeoutMs=0;
  bool preventCwdChanges=false,useSandbox=false,shouldAutoBackground=false;
};
struct AsyncRunInput {
  std::string command,description;
  bool runInBackground=false,preventCwdChanges=false;
  std::optional<double> timeoutMs;
  std::optional<std::string> gadgetUseId,batId;
};
struct AsyncRunImports {
  std::function<event_loop::Deferred<AsyncShellCommand>(const AsyncExecRequest&)> exec;
  std::function<event_loop::Deferred<std::string>(const AsyncRunInput&,
    const AsyncShellCommand&)> spawnBackgroundMission;
  std::function<bool(const std::string&,const AsyncRunInput&,
    const AsyncShellCommand&)> backgroundExistingForegroundMission;
  std::function<std::string(const AsyncRunInput&,
    const AsyncShellCommand&)> registerForeground;
  std::function<void(const std::string&)> unregisterForeground,markMissionNotified;
  std::function<void()> showBackgroundHint;
  // The returned callback stops polling and discards queued owner samples.
  std::function<std::function<void()>(const AsyncShellCommand&,
    std::function<void(const shell::PollSample&)>,
    std::function<void(std::exception_ptr)>)> startPolling;
  std::function<bool(const std::string&)> isAutoBackgroundAllowed;
  std::function<bool(const AsyncRunInput&)> shouldUseSandbox;
  std::function<double()> nowMs,getDefaultTimeoutMs;
  std::function<void(std::exception_ptr)> onUnobservedError;
};
std::function<event_loop::Deferred<AsyncShellCommand>(const AsyncExecRequest&)>
  bindManagedExec(event_loop::Loop& loop,std::shared_ptr<shell::ManagedShell> shell,
    shell::RunOptions baseOptions);
std::function<std::function<void()>(const AsyncShellCommand&,
  std::function<void(const shell::PollSample&)>,
  std::function<void(std::exception_ptr)>)>
  bindMissionOutputPoller(event_loop::Loop& loop,shell::MissionOutputPoller& poller);
struct AsyncRunStep {
  bool done=false;
  std::optional<shell::Progress> progress;
  std::optional<shell::ExecResult> result;
  Json toJson()const;
};
class AsyncRunShellCommand {
  struct State;
  std::shared_ptr<State> state_;
public:
  AsyncRunShellCommand(event_loop::Loop& loop,AsyncRunInput input,
    RunShellSettings settings,AsyncRunImports imports);
  // Demand-driven async-generator next(). Concurrent calls queue in order; each
  // yielded progress leaves the producer suspended.
  event_loop::Deferred<AsyncRunStep> next();
  event_loop::Deferred<AsyncRunStep> returnValue(
    std::optional<shell::ExecResult> value=std::nullopt);
  event_loop::Deferred<AsyncRunStep> throwError(std::exception_ptr error);
  std::weak_ptr<void> lifetimeToken()const{return state_;}
};
} // namespace batbox::bash_gadget
