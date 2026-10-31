#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <optional>

namespace batbox::cli::remote {
Json coalesceWorkerPatches(const Json& base,const Json& overlay);
struct WorkerStateConfig {
  std::function<event_loop::Deferred<bool>(const Json&)> send;
  std::function<double()> randomUnit;
  double baseDelayMs=0,maxDelayMs=0,jitterMs=0;
};
class WorkerStateUploader:public std::enable_shared_from_this<
    WorkerStateUploader> {
public:
  WorkerStateUploader(event_loop::Loop& loop,WorkerStateConfig config);
  void enqueue(Json patch);
  void close();
private:
  void drain();
  void sendCurrent();
  double retryDelay()const;
  event_loop::Loop& loop_;
  WorkerStateConfig config_;
  std::optional<Json> pending_,current_;
  event_loop::Loop::TimerHandle retryTimer_;
  std::size_t failures_=0;
  bool inflight_=false,closed_=false;
};
}
