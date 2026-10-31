#pragma once
#include "fs_operations/NodeFsAsync.hpp"
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

namespace batbox::fs_operations {
class IndigenousAsyncFsHost {
public:
  explicit IndigenousAsyncFsHost(event_loop::Loop& loop,unsigned workers=4);
  ~IndigenousAsyncFsHost();
  IndigenousAsyncFsHost(const IndigenousAsyncFsHost&)=delete;
  IndigenousAsyncFsHost& operator=(const IndigenousAsyncFsHost&)=delete;
  template<class T,class Operation>event_loop::Deferred<T> submit(Operation operation){
    event_loop::Deferred<T> result(loop_);
    auto hold=std::make_shared<event_loop::Loop::KeepAlive>(loop_.hold());
    result.then([hold](const T*,const std::exception_ptr&){hold->release();});
    try{
      enqueue([result,operation=std::move(operation),hold]{
        try{result.resolve(operation());}
        catch(...){result.reject(std::current_exception());}
      });
    }catch(...){hold->release();throw;}
    return result;
  }
private:
  void enqueue(std::function<void()> operation);
  void work();
  event_loop::Loop& loop_;
  std::mutex mutex_;
  std::condition_variable ready_;
  std::deque<std::function<void()>> queue_;
  std::vector<std::thread> workers_;
  bool stopping_=false;
};

NodeFsAsyncImports indigenousNodeFsAsyncImports(event_loop::Loop& loop,
  std::shared_ptr<IndigenousAsyncFsHost> host);
}
