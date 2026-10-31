#pragma once

#include "interactive/QueryGuard.hpp"
#include "interactive/submit/CommandQueue.hpp"
#include <memory>

namespace batbox::interactive::submit {

class QueueReadiness:public std::enable_shared_from_this<QueueReadiness> {
public:
  QueueReadiness(event_loop::Loop& owner,CommandQueue& queue,QueryGuard& guard,
                 std::function<bool()> hasActiveLocalUi,
                 ExecuteInput executeInput,
                 std::function<void(std::exception_ptr)> onError);
  ~QueueReadiness();
  void start();
  void stop();
  void check();
  void refresh(){schedule();}
private:
  void schedule();
  event_loop::Loop& owner_;
  CommandQueue& queue_;
  QueryGuard& guard_;
  std::function<bool()> hasActiveLocalUi_;
  ExecuteInput executeInput_;
  std::function<void(std::exception_ptr)> onError_;
  std::function<void()> unsubscribeQueue_,unsubscribeGuard_;
  bool started_=false,scheduled_=false;
};

} // namespace batbox::interactive::submit
