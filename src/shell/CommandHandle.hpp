#pragma once
#include "shell/ManagedShell.hpp"
#include <future>
#include <memory>
#include <string>

namespace batbox::shell {
// Handle over an Indigenous managed child process. `result`
// resolves after child exit; `background` preserves the original mission ID.
class CommandHandle {
public:
  struct LargeOutputInfo {std::string path;std::size_t size;std::string missionId;};
  using TimeoutCallback=std::function<void(const std::function<bool(const std::string&)>&)>;
  struct State;
  explicit CommandHandle(std::shared_ptr<State> state):state_(std::move(state)){}
  const std::string& missionId()const;
  std::string outputPath()const;
  std::optional<LargeOutputInfo> largeOutputInfo()const;
  std::shared_future<ExecResult> result()const;
  CommandStatus status()const;
  bool background(const std::string& missionId,bool byUser=false)const;
  void kill()const;
  void cleanup()const;
  void onTimeout(TimeoutCallback callback)const;
  std::function<bool(const std::string&)> weakBackgroundCallback()const;
  // Indigenous lifetime probe. Does not expose the internal command state.
  std::weak_ptr<void> lifetimeToken()const{return state_;}
private:
  std::shared_ptr<State> state_;
};
CommandHandle startManagedCommand(std::shared_ptr<ManagedShell> shell,RunOptions options,
  bool expectTimeoutCallback=false);
} // namespace batbox::shell
