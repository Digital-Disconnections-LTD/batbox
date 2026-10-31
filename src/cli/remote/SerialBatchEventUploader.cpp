#include "cli/remote/SerialBatchEventUploader.hpp"
#include <algorithm>
#include <cmath>
#include <utility>

namespace batbox::cli::remote {
using event_loop::Deferred;
using event_loop::Unit;
SerialBatchEventUploader::SerialBatchEventUploader(
    event_loop::Loop& loop,SerialBatchConfig config):loop_(loop),
    config_(std::move(config)){
  if(!config_.send||!config_.randomUnit||!config_.jsonStringify||
     config_.maxBatchSize==0||config_.maxQueueSize==0)
    throw std::invalid_argument("Serial batch uploader configuration required");
}
Deferred<Unit> SerialBatchEventUploader::enqueue(Json item){
  return enqueue(std::vector<Json>{std::move(item)});
}
Deferred<Unit> SerialBatchEventUploader::enqueue(std::vector<Json> items){
  Deferred<Unit> result(loop_);
  if(closed_||items.empty()){result.resolve({});return result;}
  resumeEnqueue(WaitingEnqueue{std::move(items),result});
  return result;
}
void SerialBatchEventUploader::resumeEnqueue(WaitingEnqueue waiting){
  if(closed_){waiting.result.resolve({});return;}
  if(pending_.size()+waiting.items.size()>config_.maxQueueSize){
    backpressure_.push_back(std::move(waiting));return;
  }
  for(auto& item:waiting.items)pending_.push_back(std::move(item));
  drain();
  waiting.result.resolve({});
}
Deferred<Unit> SerialBatchEventUploader::flush(){
  Deferred<Unit> result(loop_);
  if(pending_.empty()&&!draining_){result.resolve({});return result;}
  drain();flushWaiters_.push_back(result);
  return result;
}
void SerialBatchEventUploader::close(){
  if(closed_)return;
  closed_=true;pendingAtClose_=pending_.size();pending_.clear();
  const bool sleeping=sleepTimer_.valid();
  if(sleeping)sleepTimer_.cancel();
  sleepTimer_={};
  if(sleeping){
    const std::weak_ptr<SerialBatchEventUploader> weak=shared_from_this();
    loop_.queueMicrotask([weak]{
      if(const auto self=weak.lock())self->finishDrain();
    });
  }
  auto blocked=std::move(backpressure_);
  for(auto& waiting:blocked)waiting.result.resolve({});
  auto waiters=std::move(flushWaiters_);
  for(const auto& result:waiters)result.resolve({});
}
void SerialBatchEventUploader::releaseBackpressure(){
  auto blocked=std::move(backpressure_);
  const std::weak_ptr<SerialBatchEventUploader> weak=shared_from_this();
  for(auto& waiting:blocked){
    loop_.queueMicrotask([weak,waiting=std::move(waiting)]()mutable{
      if(const auto self=weak.lock())self->resumeEnqueue(std::move(waiting));
      else waiting.result.resolve({});
    });
  }
}
void SerialBatchEventUploader::drain(){
  if(draining_||closed_)return;
  draining_=true;failures_=0;step();
}
std::vector<Json> SerialBatchEventUploader::takeBatch(){
  std::vector<Json> batch;
  std::size_t bytes=0;
  while(!pending_.empty()&&batch.size()<config_.maxBatchSize){
    std::size_t itemBytes=0;
    if(config_.maxBatchBytes){
      try{itemBytes=config_.jsonStringify(pending_.front()).size();}
      catch(...){pending_.pop_front();continue;}
      if(!batch.empty()&&bytes+itemBytes>*config_.maxBatchBytes)break;
    }
    bytes+=itemBytes;
    batch.push_back(std::move(pending_.front()));pending_.pop_front();
  }
  return batch;
}
double SerialBatchEventUploader::retryDelay(std::size_t failures,
    std::optional<double> retryAfterMs)const{
  const double jitter=config_.randomUnit()*config_.jitterMs;
  if(retryAfterMs)return std::max(config_.baseDelayMs,
    std::min(*retryAfterMs,config_.maxDelayMs))+jitter;
  return std::min(config_.baseDelayMs*std::pow(2.0,
    static_cast<double>(failures-1)),config_.maxDelayMs)+jitter;
}
void SerialBatchEventUploader::step(){
  if(closed_||pending_.empty()){finishDrain();return;}
  inFlight_=takeBatch();
  if(inFlight_.empty()){step();return;}
  try{
    auto result=config_.send(inFlight_);
    const std::weak_ptr<SerialBatchEventUploader> weak=shared_from_this();
    result.then([weak](const Unit*,const std::exception_ptr& error){
      const auto self=weak.lock();if(!self)return;
      if(!error){
        self->inFlight_.clear();self->failures_=0;
        self->releaseBackpressure();self->step();return;
      }
      std::optional<double> retryAfter;
      try{std::rethrow_exception(error);}
      catch(const RetryableError& failed){retryAfter=failed.retryAfterMs();}
      catch(...){}
      ++self->failures_;
      if(self->config_.maxConsecutiveFailures&&
         self->failures_>=*self->config_.maxConsecutiveFailures){
        ++self->droppedBatches_;
        if(self->config_.onBatchDropped)
          self->config_.onBatchDropped(self->inFlight_.size(),self->failures_);
        self->inFlight_.clear();self->failures_=0;
        self->releaseBackpressure();self->step();return;
      }
      for(auto it=self->inFlight_.rbegin();it!=self->inFlight_.rend();++it)
        self->pending_.push_front(*it);
      self->inFlight_.clear();
      const auto delay=self->retryDelay(self->failures_,retryAfter);
      self->sleepTimer_=self->loop_.setTimeout(delay,[weak]{
        if(const auto current=weak.lock()){
          current->sleepTimer_={};current->step();
        }
      });
    });
  }catch(...){
    // A synchronous exception is the same failed send as a rejected promise.
    Deferred<Unit> rejected(loop_);rejected.reject(std::current_exception());
    const std::weak_ptr<SerialBatchEventUploader> weak=shared_from_this();
    rejected.then([weak](const Unit*,const std::exception_ptr& error){
      if(const auto self=weak.lock()){
        ++self->failures_;
        if(self->config_.maxConsecutiveFailures&&
           self->failures_>=*self->config_.maxConsecutiveFailures){
          ++self->droppedBatches_;
          if(self->config_.onBatchDropped)
            self->config_.onBatchDropped(self->inFlight_.size(),self->failures_);
          self->inFlight_.clear();self->failures_=0;
          self->releaseBackpressure();self->step();return;
        }
        for(auto it=self->inFlight_.rbegin();it!=self->inFlight_.rend();++it)
          self->pending_.push_front(*it);
        self->inFlight_.clear();
        std::optional<double> retryAfter;
        try{std::rethrow_exception(error);}
        catch(const RetryableError& failure){retryAfter=failure.retryAfterMs();}
        catch(...){}
        const auto delay=self->retryDelay(self->failures_,retryAfter);
        self->sleepTimer_=self->loop_.setTimeout(delay,[weak]{
          if(const auto current=weak.lock()){
            current->sleepTimer_={};current->step();
          }
        });
      }
    });
  }
}
void SerialBatchEventUploader::finishDrain(){
  draining_=false;
  if(pending_.empty()){
    auto waiters=std::move(flushWaiters_);
    for(const auto& result:waiters)result.resolve({});
  }
}
}
