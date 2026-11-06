#include "fs_operations/NodeFsAsyncIndigenous.hpp"
#include "runtime/AsyncLocalStorage.hpp"
#include <stdexcept>

namespace batbox::fs_operations {
IndigenousAsyncFsHost::IndigenousAsyncFsHost(event_loop::Loop& loop,unsigned workers):loop_(loop){
  if(workers==0)throw std::invalid_argument("At least one file worker is required");
  workers_.reserve(workers);
  for(unsigned i=0;i<workers;++i)workers_.emplace_back([this]{work();});
}
IndigenousAsyncFsHost::~IndigenousAsyncFsHost(){
  {std::lock_guard lock(mutex_);stopping_=true;}
  ready_.notify_all();
  for(auto& worker:workers_)worker.join();
}
void IndigenousAsyncFsHost::enqueue(std::function<void()> operation){
  {std::lock_guard lock(mutex_);
    if(stopping_)throw std::logic_error("Filesystem workers stopped");
    queue_.push_back(runtime::bindAsyncContext(std::move(operation)));}
  ready_.notify_one();
}
void IndigenousAsyncFsHost::work(){
  for(;;){
    std::function<void()> operation;
    {std::unique_lock lock(mutex_);
      ready_.wait(lock,[this]{return stopping_||!queue_.empty();});
      if(queue_.empty())return;
      operation=std::move(queue_.front());queue_.pop_front();}
    operation();
  }
}
}
