#pragma once
#include "context/Context.hpp"
#include "event_loop/Deferred.hpp"

namespace batbox::context {
struct AsyncServices : ContextInputs {
  std::function<event_loop::Deferred<bool>()> getIsGit;
  std::function<event_loop::Deferred<std::string>()> getBranch,getDefaultBranch;
  std::function<event_loop::Deferred<std::string>(const std::vector<std::string>&)>
    gitStdoutNoThrow;
  std::function<event_loop::Deferred<MemoryFiles>()> getMemoryFiles;
};
// Promise-valued memoization: repeated calls return the same in-flight or
// rejected Promise. Clearing a stash leaves already returned Promises alive.
class AsyncContextStash {
  struct State;
  std::shared_ptr<State> state_;
public:
  explicit AsyncContextStash(event_loop::Loop&);
  event_loop::Deferred<std::optional<std::string>> getGitStatus(const AsyncServices&);
  event_loop::Deferred<Json> getSystemContext(const AsyncServices&);
  event_loop::Deferred<Json> getUserContext(const AsyncServices&);
  std::optional<std::string> getSystemPoetryInjection()const;
  void setSystemPoetryInjection(std::optional<std::string>);
  void clearGitStatusStash();
  void clearSystemContextStash();
  void clearUserContextStash();
};
}
