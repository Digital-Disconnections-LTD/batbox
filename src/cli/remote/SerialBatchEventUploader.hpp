#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <deque>
#include <optional>
#include <stdexcept>
#include <vector>

namespace batbox::cli::remote {
class RetryableError:public std::runtime_error {
public:
  RetryableError(std::string message,std::optional<double> retryAfterMs={}):
    std::runtime_error(std::move(message)),retryAfterMs_(retryAfterMs){}
  std::optional<double> retryAfterMs()const{return retryAfterMs_;}
private:
  std::optional<double> retryAfterMs_;
};
struct SerialBatchConfig {
  std::size_t maxBatchSize=1,maxQueueSize=1;
  std::optional<std::size_t> maxBatchBytes;
  double baseDelayMs=0,maxDelayMs=0,jitterMs=0;
  std::optional<std::size_t> maxConsecutiveFailures;
  std::function<event_loop::Deferred<event_loop::Unit>(const std::vector<Json>&)>
    send;
  std::function<void(std::size_t,std::size_t)> onBatchDropped;
  std::function<double()> randomUnit;
  std::function<std::string(const Json&)> jsonStringify;
};
class SerialBatchEventUploader:public std::enable_shared_from_this<
    SerialBatchEventUploader> {
public:
  SerialBatchEventUploader(event_loop::Loop& loop,SerialBatchConfig config);
  event_loop::Deferred<event_loop::Unit> enqueue(std::vector<Json> items);
  event_loop::Deferred<event_loop::Unit> enqueue(Json item);
  event_loop::Deferred<event_loop::Unit> flush();
  void close();
  std::size_t droppedBatchCount()const{return droppedBatches_;}
  std::size_t pendingCount()const{return closed_?pendingAtClose_:pending_.size();}
private:
  struct WaitingEnqueue {
    std::vector<Json> items;
    event_loop::Deferred<event_loop::Unit> result;
  };
  void resumeEnqueue(WaitingEnqueue waiting);
  void releaseBackpressure();
  void drain();
  void step();
  void finishDrain();
  std::vector<Json> takeBatch();
  double retryDelay(std::size_t failures,
    std::optional<double> retryAfterMs)const;
  event_loop::Loop& loop_;
  SerialBatchConfig config_;
  std::deque<Json> pending_;
  std::vector<WaitingEnqueue> backpressure_;
  std::vector<event_loop::Deferred<event_loop::Unit>> flushWaiters_;
  std::vector<Json> inFlight_;
  event_loop::Loop::TimerHandle sleepTimer_;
  bool draining_=false,closed_=false;
  std::size_t pendingAtClose_=0,droppedBatches_=0,failures_=0;
};
}
