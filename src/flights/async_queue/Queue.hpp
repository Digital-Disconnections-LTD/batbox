#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <exception>
#include <functional>
#include <memory>
#include <string>

namespace batbox::flights::async_queue {
using EntryReader=std::function<Json()>;
using Completion=event_loop::Deferred<event_loop::Unit>;
struct QueueImports {
  std::function<Completion(const std::string&,const std::string&)> appendFile;
  std::function<Completion(const std::string&)> mkdir;
  std::function<void(std::exception_ptr)> onBackgroundError;
};

// Project queue state. All queue operations run on the Loop owner; filesystem
// operations return loop-bound async completions.
class Queue {
  struct State;
public:
  Queue(event_loop::Loop& loop,QueueImports imports,double flushIntervalMs=100,
    std::size_t maxChunkUnits=100ull*1024*1024);
  Completion enqueueWrite(const std::string& file,EntryReader entry);
  Completion enqueueWrite(const std::string& file,Json entry);
  Completion flush();
  void incrementPendingWrites();
  void decrementPendingWrites();
  event_loop::Loop::Dispatcher dispatcher()const;

  template<class T>event_loop::Deferred<T> trackWrite(
    const std::function<event_loop::Deferred<T>()>& operation){
    event_loop::Deferred<T> result(dispatcher());
    incrementPendingWrites();
    auto state=state_;
    try{
      auto pending=operation();
      pending.then([state,result](const T* value,const std::exception_ptr& error){
        decrementState(state);
        if(error)result.reject(error);else result.resolve(*value);
      });
    }catch(...){decrementPendingWrites();result.reject(std::current_exception());}
    return result;
  }
private:
  std::shared_ptr<State> state_;
  static void decrementState(const std::shared_ptr<State>& state);
};

// Real POSIX async file operations: worker threads do blocking syscalls and
// post completion to the owner loop while a referenced KeepAlive is held.
QueueImports indigenousQueueImports(event_loop::Loop& loop,
  std::function<void(std::exception_ptr)> onBackgroundError);
} // namespace batbox::flights::async_queue
