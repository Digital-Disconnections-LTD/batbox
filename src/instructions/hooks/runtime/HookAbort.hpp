#pragma once
#include "event_loop/Loop.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>

namespace batbox::instructions::hooks::runtime {
class HookAbort : public std::enable_shared_from_this<HookAbort> {
public:
  static std::shared_ptr<HookAbort> create(event_loop::Loop& loop,
    std::stop_token parent,double timeoutMs);
  std::stop_token signal()const{return source_.get_token();}
  void cleanup();
private:
  std::stop_source source_;
  std::optional<event_loop::Loop::TimerHandle> timer_;
  std::unique_ptr<std::stop_callback<std::function<void()>>> parentCallback_;
};
} // namespace batbox::instructions::hooks::runtime
