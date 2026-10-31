#pragma once
#include "event_loop/Loop.hpp"
#include <functional>
#include <memory>

namespace batbox::cleanup {
// POSIX signal actions write one signal byte to a nonblocking self-pipe.
// A reader thread posts callbacks to the long-lived event-loop owner.
// One process bridge is active at a time; destroy it before its Loop.
class IndigenousSignalBridge {
public:
  explicit IndigenousSignalBridge(event_loop::Loop& loop);
  ~IndigenousSignalBridge();
  IndigenousSignalBridge(const IndigenousSignalBridge&)=delete;
  IndigenousSignalBridge& operator=(const IndigenousSignalBridge&)=delete;
  void onSignal(int signal,std::function<void()> callback);
private:
  struct State;
  std::shared_ptr<State> state_;
};
} // namespace batbox::cleanup
