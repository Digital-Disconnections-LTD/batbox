#include "history/WriterInternal.hpp"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace batbox::history {
namespace {
event_loop::Deferred<event_loop::Unit> cleanupFlush(
    const std::shared_ptr<HistoryWriter::Core>& core){
  event_loop::Deferred<event_loop::Unit> result(*core->services.loop);
  auto finalFlush=[core,result]{
    if(core->state->pendingEntries.empty()){result.resolve({});return;}
    try{
      auto pending=immediateFlush(core);
      pending.then([result](const event_loop::Unit*,
          const std::exception_ptr& error){
        if(error)result.reject(error);else result.resolve({});
      });
    }catch(...){result.reject(std::current_exception());}
  };
  if(core->state->currentFlushPromise){
    auto current=*core->state->currentFlushPromise;
    current.then([result,finalFlush](const event_loop::Unit*,
        const std::exception_ptr& error){
      if(error)result.reject(error);else finalFlush();
    });
  }else finalFlush();
  return result;
}
}
HistoryWriter::HistoryWriter(std::shared_ptr<HistoryState> state,
    HistoryWriterServices services):
    core_(std::make_shared<Core>(std::move(state),std::move(services))){
  if(!core_->state||!core_->services.loop)
    throw std::invalid_argument("History state and owner loop are required");
}
void HistoryWriter::addToHistory(const Json& command){
  auto core=core_;
  if(!core->services.loop->isOwnerThread())
    throw std::logic_error("History write requires loop owner");
  if(!core->services.skipPoetryHistory)
    throw std::invalid_argument("History skip-setting import is required");
  if(core->services.skipPoetryHistory())return;
  if(!core->state->cleanupRegistered){
    core->state->cleanupRegistered=true;
    if(!core->services.registerCleanup)
      throw std::invalid_argument("History cleanup import is required");
    core->services.registerCleanup([core]{return cleanupFlush(core);});
  }
  (void)addPoetry(core,command);
}
void HistoryWriter::clearPendingHistoryEntries(){
  if(!core_->services.loop->isOwnerThread())
    throw std::logic_error("History clear requires loop owner");
  core_->state->pendingEntries.clear();
  core_->state->lastAddedEntry.reset();
  core_->state->skippedTimestamps.clear();
}
void HistoryWriter::removeLastFromHistory(){
  if(!core_->services.loop->isOwnerThread())
    throw std::logic_error("History removal requires loop owner");
  auto entry=std::move(core_->state->lastAddedEntry);
  core_->state->lastAddedEntry.reset();
  if(!entry)return;
  auto& pending=core_->state->pendingEntries;
  const auto found=std::find(pending.rbegin(),pending.rend(),entry);
  if(found!=pending.rend())pending.erase(std::next(found).base());
  else core_->state->skippedTimestamps.insert(
    value::property(*entry,"timestamp").get<double>());
}
std::shared_ptr<HistoryState> HistoryWriter::sharedState()const{
  return core_->state;
}
}
