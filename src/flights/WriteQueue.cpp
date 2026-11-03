#include "flights/WriteQueue.hpp"
#include "runtime/Paths.hpp"
#include <algorithm>
#include <filesystem>
namespace batbox::flights {
WriteQueue::WriteQueue(QueueServices services,Duration interval,std::size_t maxChunkUnits)
  :services(std::move(services)),interval(interval),maxChunkUnits(maxChunkUnits),worker([this]{run();}){}
WriteQueue::~WriteQueue() {
  // The application calls flush explicitly to observe failures before exit.
  try{flush();}catch(...){}
  {std::lock_guard lock(mutex);stopping=true;wake.notify_all();}
  worker.join();
}
std::shared_future<void> WriteQueue::enqueueWrite(const std::string& file,Json entry) {
  return enqueueWrite(file,[value=std::move(entry)]{return value;});
}
std::shared_future<void> WriteQueue::enqueueWrite(const std::string& file,EntryReader read) {
  auto promise=std::make_shared<std::promise<void>>();auto future=promise->get_future().share();
  std::lock_guard lock(mutex);
  auto at=std::find_if(queues.begin(),queues.end(),[&](const auto& q){return q.file==file;});
  if(at==queues.end()){queues.push_back({file,{}});at=std::prev(queues.end());}
  at->entries.push_back({std::move(read),std::move(promise)});scheduleDrain();return future;
}
void WriteQueue::scheduleDrain() {
  if(timer)return;
  timer=std::chrono::steady_clock::now()+interval;wake.notify_all();
}
void WriteQueue::incrementPendingWrites(){std::lock_guard lock(mutex);++pendingWriteCount;}
void WriteQueue::decrementPendingWrites(){std::lock_guard lock(mutex);--pendingWriteCount;if(pendingWriteCount==0)finished.notify_all();}
void WriteQueue::appendToFile(const std::string& file,const std::string& content) {
  try{services.appendFile(file,content);}
  catch(...){services.mkdir(value::dirname(file));services.appendFile(file,content);}
}
void WriteQueue::drainWriteQueue() {
  // Include files added while an append is pending. A stable list and a short
  // producer lock keep queue traversal valid.
  std::unique_lock lock(mutex);
  for(auto at=queues.begin();at!=queues.end();++at) {
    if(at->entries.empty())continue;
    const auto file=at->file;auto batch=std::move(at->entries);at->entries.clear();lock.unlock();
    std::string content;std::size_t units=0;
    std::vector<std::shared_ptr<std::promise<void>>> resolvers;
    try {for(const auto& entry:batch) {
      const auto line=value::stringify(entry.read())+'\n';const auto lineUnits=value::stringLength(line);
      if(units+lineUnits>=maxChunkUnits) {
        appendToFile(file,content);for(const auto& resolver:resolvers)resolver->set_value();
        resolvers.clear();content.clear();units=0;
      }
      content+=line;units+=lineUnits;resolvers.push_back(entry.completion);
    }
    if(!content.empty()){appendToFile(file,content);for(const auto& resolver:resolvers)resolver->set_value();}
    }catch(...) {
      // Enqueue promises remain pending when the drain rejects. Keep them alive
      // until shutdown rather than reporting broken_promise.
      lock.lock();for(const auto& entry:batch)failedCompletions.push_back(entry.completion);throw;
    }
    lock.lock();
  }
  queues.remove_if([](const auto& queue){return queue.entries.empty();});
}
void WriteQueue::run() {
  std::unique_lock lock(mutex);
  while(!stopping) {
    wake.wait(lock,[&]{return stopping || (!failure && (immediate || timer.has_value()));});
    if(stopping)break;
    if(!immediate && timer && std::chrono::steady_clock::now()<*timer) {
      const auto deadline=*timer;
      wake.wait_until(lock,deadline,[&]{return stopping || immediate;});
      if(stopping)break;
    }
    timer.reset();immediate=false;active=true;lock.unlock();
    std::exception_ptr error;try{drainWriteQueue();}catch(...){error=std::current_exception();}
    lock.lock();active=false;++completedDrains;
    if(error)failure=error;
    else if(!queues.empty())scheduleDrain();
    finished.notify_all();
    if(error && services.onBackgroundError) {
      lock.unlock();try{services.onBackgroundError(error);}catch(...){}lock.lock();
    }
  }
}
void WriteQueue::flush() {
  std::unique_lock lock(mutex);timer.reset();
  if(failure)std::rethrow_exception(failure);
  if(active)finished.wait(lock,[&]{return !active || failure;});
  if(failure)std::rethrow_exception(failure);
  if(!queues.empty()) {
    const auto target=completedDrains+1;
    immediate=true;wake.notify_all();
    finished.wait(lock,[&]{return failure || completedDrains>=target;});
  }
  if(failure)std::rethrow_exception(failure);
  finished.wait(lock,[&]{return pendingWriteCount==0;});
}
}
