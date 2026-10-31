#pragma once
#include "instructions/hooks/runtime/AsyncHookRoster.hpp"
#include "instructions/hooks/runtime/IndigenousCommandProcess.hpp"
#include <memory>

namespace batbox::instructions::hooks::runtime {
// Indigenous child/pipe boundary for hook commands. It supplies ShellCommand
// methods while a dedicated pipe worker posts stream events to the caller-owned
// loop. The AsyncHookRoster remains the mission owner.
class IndigenousManagedHookProcess {
public:
  struct State;
  explicit IndigenousManagedHookProcess(std::shared_ptr<State> state):state_(std::move(state)){}
  long pid()const;
  std::string status()const;
  bool background(const std::string& processId)const;
  void kill()const;
  void cleanup()const;
  event_loop::Deferred<std::string> getStdout()const;
  std::string getStderr()const;
  event_loop::Deferred<int> resultCode()const;
  event_loop::Deferred<int> closeCode()const;
  void writeStdin(const std::string& text)const;
  void destroyStdin()const;
  std::shared_ptr<AsyncHookProcess> rosterProcess()const;
  // Indigenous stream listeners run on the owner loop in pipe read order.
  void onStdout(std::function<void(const std::string&)> callback)const;
  void onStderr(std::function<void(const std::string&)> callback)const;
  std::optional<std::string> stdinError()const;
  std::weak_ptr<void> lifetimeToken()const{return state_;}
private:
  std::shared_ptr<State> state_;
};
IndigenousManagedHookProcess startIndigenousManagedHookProcess(
  event_loop::Loop& loop,IndigenousProcessRequest request,bool keepStdinOpen=false,
  std::function<std::string()> currentAbortReason={});
} // namespace batbox::instructions::hooks::runtime
