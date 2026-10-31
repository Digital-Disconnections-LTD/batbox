#pragma once
#include "event_loop/Loop.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>

namespace batbox::instructions::hooks::runtime {
class CombinedAbort final : public std::enable_shared_from_this<CombinedAbort> {
public:
  static std::shared_ptr<CombinedAbort> create(event_loop::Loop& loop,
    std::optional<std::stop_token> signal=std::nullopt,
    std::optional<std::stop_token> signalB=std::nullopt,
    std::optional<double> timeoutMs=std::nullopt);
  std::stop_token signal()const{return source_.get_token();}
  bool aborted()const{return source_.stop_requested();}
  // Mirrors the returned cleanup closure. Owner-thread only.
  void cleanup();
private:
  std::stop_source source_;
  std::optional<event_loop::Loop::TimerHandle> timer_;
  std::unique_ptr<std::stop_callback<std::function<void()>>> parentA_,parentB_;
  event_loop::Loop::Dispatcher dispatcher_;
  bool cleaned_=false;
  void abortFromParent();
};
} // namespace batbox::instructions::hooks::runtime
