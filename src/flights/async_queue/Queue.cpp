#include "flights/async_queue/Queue.hpp"
#include "runtime/Paths.hpp"
#include <deque>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace batbox::flights::async_queue {
struct Queue::State:std::enable_shared_from_this<State> {
  struct Queued {EntryReader read;Completion completed;};
  struct FileQueue {std::string file;std::deque<Queued> entries;bool present=true;};
  struct Drain:std::enable_shared_from_this<Drain> {
    std::shared_ptr<State> owner;
    Completion completed;
    std::size_t index=0,offset=0,contentUnits=0;
    std::shared_ptr<FileQueue> file;
    std::deque<Queued> batch;
    std::string content;
    std::vector<Completion> resolvers;
    bool done=false;
    explicit Drain(std::shared_ptr<State> state):owner(std::move(state)),completed(owner->dispatcher){}
    void fail(std::exception_ptr error){
      if(done)return;
      done=true;
      owner->finishDrain();
      completed.reject(std::move(error));
    }
    void finish(){
      if(done)return;
      done=true;
      owner->cleanupEmptyQueues();
      owner->finishDrain();
      completed.resolve({});
    }
    void addLine(std::string line,const Completion& resolver){
      contentUnits+=value::stringLength(line);content+=line;resolvers.push_back(resolver);++offset;
    }
    void step(){
      try{
        for(;;){
          if(file){
            if(offset<batch.size()){
              const auto line=value::stringify(batch[offset].read())+'\n';
              const auto resolver=batch[offset].completed;
              if(contentUnits+value::stringLength(line)>=owner->maxChunkUnits){
                auto written=std::move(resolvers);resolvers.clear();
                auto previous=std::move(content);content.clear();contentUnits=0;
                auto self=shared_from_this();
                owner->appendToFile(file->file,previous).then(
                  [self,written=std::move(written),line,resolver](
                    const event_loop::Unit*,const std::exception_ptr& error)mutable{
                    if(error){self->fail(error);return;}
                    for(const auto& ready:written)ready.resolve({});
                    self->addLine(line,resolver);self->step();
                  });
                return;
              }
              addLine(line,resolver);continue;
            }
            if(!content.empty()){
              auto written=std::move(resolvers);resolvers.clear();
              auto previous=std::move(content);content.clear();contentUnits=0;
              auto self=shared_from_this();
              owner->appendToFile(file->file,previous).then(
                [self,written=std::move(written)](
                  const event_loop::Unit*,const std::exception_ptr& error){
                  if(error){self->fail(error);return;}
                  for(const auto& ready:written)ready.resolve({});
                  self->file.reset();self->batch.clear();self->offset=0;self->step();
                });
              return;
            }
            file.reset();batch.clear();offset=0;continue;
          }
          if(index>=owner->order.size()){finish();return;}
          auto next=owner->order[index++];
          if(!next->present||next->entries.empty())continue;
          file=std::move(next);batch=std::move(file->entries);file->entries.clear();
          offset=0;content.clear();contentUnits=0;resolvers.clear();
        }
      }catch(...){fail(std::current_exception());}
    }
  };
  event_loop::Loop& loop;
  event_loop::Loop::Dispatcher dispatcher;
  QueueImports imports;
  double flushIntervalMs;
  std::size_t maxChunkUnits;
  std::vector<std::shared_ptr<FileQueue>> order;
  std::unordered_map<std::string,std::shared_ptr<FileQueue>> byFile;
  std::optional<event_loop::Loop::TimerHandle> flushTimer;
  std::optional<Completion> activeDrain;
  std::vector<Completion> flushResolvers;
  long long pendingWriteCount=0;
  std::size_t drainsInFlight=0;
  State(event_loop::Loop& loop,QueueImports imports,double interval,std::size_t maxChunk):
    loop(loop),dispatcher(loop.dispatcher()),imports(std::move(imports)),
    flushIntervalMs(interval),maxChunkUnits(maxChunk){
    if(!this->imports.appendFile||!this->imports.mkdir||!this->imports.onBackgroundError)
      throw std::invalid_argument("async queue append/mkdir/diagnostic imports required");
    if(maxChunkUnits==0)throw std::invalid_argument("maxChunkUnits must be positive");
  }
  void assertOwner()const{
    if(!dispatcher.isOwnerThread())throw std::logic_error("async queue owner thread required");
  }
  Completion appendToFile(const std::string& file,const std::string& data){
    Completion output(dispatcher);auto self=shared_from_this();
    try{
      imports.appendFile(file,data).then([self,output,file,data](
        const event_loop::Unit*,const std::exception_ptr& error){
        if(!error){output.resolve({});return;}
        try{
          self->imports.mkdir(value::dirname(file)).then([self,output,file,data](
            const event_loop::Unit*,const std::exception_ptr& mkdirError){
            if(mkdirError){output.reject(mkdirError);return;}
            try{self->imports.appendFile(file,data).then([output](
              const event_loop::Unit*,const std::exception_ptr& retryError){
              if(retryError)output.reject(retryError);else output.resolve({});});}
            catch(...){output.reject(std::current_exception());}
          });
        }catch(...){output.reject(std::current_exception());}
      });
    }catch(...){output.reject(std::current_exception());}
    return output;
  }
  Completion drainWriteQueue(){
    assertOwner();++drainsInFlight;
    auto drain=std::make_shared<Drain>(shared_from_this());
    auto result=drain->completed;drain->step();return result;
  }
  void finishDrain(){
    if(drainsInFlight>0)--drainsInFlight;
    if(drainsInFlight==0){
      std::vector<std::shared_ptr<FileQueue>> retained;
      retained.reserve(byFile.size());
      for(auto& queue:order)if(queue->present)retained.push_back(std::move(queue));
      order=std::move(retained);
    }
  }
  void cleanupEmptyQueues(){
    for(auto& queue:order)if(queue->present&&queue->entries.empty()){
      queue->present=false;byFile.erase(queue->file);
    }
  }
  void scheduleDrain(){
    assertOwner();if(flushTimer)return;
    auto self=shared_from_this();
    flushTimer=loop.setTimeout(flushIntervalMs,[self]{
      self->flushTimer.reset();
      auto drain=self->drainWriteQueue();self->activeDrain=drain;
      drain.then([self](const event_loop::Unit*,const std::exception_ptr& error){
        if(error){self->imports.onBackgroundError(error);return;}
        self->activeDrain.reset();
        if(!self->byFile.empty())self->scheduleDrain();
      });
    });
  }
  void decrementPendingWrites(){
    assertOwner();--pendingWriteCount;
    if(pendingWriteCount==0){
      auto resolvers=std::move(flushResolvers);flushResolvers.clear();
      for(const auto& resolver:resolvers)resolver.resolve({});
    }
  }
};

Queue::Queue(event_loop::Loop& loop,QueueImports imports,double interval,std::size_t maxChunk):
  state_(std::make_shared<State>(loop,std::move(imports),interval,maxChunk)){}
event_loop::Loop::Dispatcher Queue::dispatcher()const{return state_->dispatcher;}
Completion Queue::enqueueWrite(const std::string& file,EntryReader entry){
  auto state=state_;state->assertOwner();
  if(!entry)throw std::invalid_argument("entry reader required");
  Completion result(state->dispatcher);
  auto at=state->byFile.find(file);
  if(at==state->byFile.end()){
    auto queue=std::make_shared<State::FileQueue>();queue->file=file;
    state->order.push_back(queue);state->byFile.emplace(file,queue);at=state->byFile.find(file);
  }
  at->second->entries.push_back({std::move(entry),result});
  state->scheduleDrain();return result;
}
Completion Queue::enqueueWrite(const std::string& file,Json entry){
  return enqueueWrite(file,[value=std::move(entry)]{return value;});
}
Completion Queue::flush(){
  auto state=state_;state->assertOwner();
  if(state->flushTimer){state->flushTimer->cancel();state->flushTimer.reset();}
  Completion result(state->dispatcher);
  std::function<void()> drainRemaining=[state,result]{
    auto pending=state->drainWriteQueue();
    pending.then([state,result](const event_loop::Unit*,const std::exception_ptr& error){
      if(error){result.reject(error);return;}
      if(state->pendingWriteCount==0)result.resolve({});
      else {
        Completion awaited(state->dispatcher);
        awaited.then([result](const event_loop::Unit*,const std::exception_ptr& error){
          if(error)result.reject(error);else result.resolve({});
        });
        state->flushResolvers.push_back(awaited);
      }
    });
  };
  if(state->activeDrain){
    state->activeDrain->then([drainRemaining,result](
      const event_loop::Unit*,const std::exception_ptr& error){
      if(error)result.reject(error);else drainRemaining();});
  }else drainRemaining();
  return result;
}
void Queue::incrementPendingWrites(){state_->assertOwner();++state_->pendingWriteCount;}
void Queue::decrementState(const std::shared_ptr<State>& state){state->decrementPendingWrites();}
void Queue::decrementPendingWrites(){decrementState(state_);}
} // namespace batbox::flights::async_queue
