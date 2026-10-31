#include "history/Views.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::history {
struct HistorySimpleCursor::State {
  explicit State(HistoryViewImports value):imports(std::move(value)){}
  HistoryViewImports imports;
  std::optional<LogEntryCursor> reader;
  std::optional<event_loop::Deferred<std::optional<Json>>> pending;
  std::optional<event_loop::Deferred<event_loop::Unit>> closingResult;
  bool started=false,done=false,closing=false,returnRequested=false;
};
namespace {
using State=HistorySimpleCursor::State;
void closeReader(const std::shared_ptr<State>& state,std::exception_ptr primary);
void emit(const std::shared_ptr<State>& state,std::optional<Json> value){
  auto pending=std::move(state->pending);state->pending.reset();
  if(pending)pending->resolve(std::move(value));
  if(pending&&state->returnRequested&&!state->closing&&!state->done)
    closeReader(state,{});
}
void reject(const std::shared_ptr<State>& state,std::exception_ptr error){
  state->done=true;
  auto pending=std::move(state->pending);state->pending.reset();
  if(pending)pending->reject(error);
  if(state->closingResult){
    if(pending&&state->returnRequested)state->closingResult->resolve({});
    else state->closingResult->reject(error);
    state->closingResult.reset();
  }
}
void complete(const std::shared_ptr<State>& state){
  state->done=true;emit(state,std::nullopt);
  if(state->closingResult){state->closingResult->resolve({});
    state->closingResult.reset();}
}
void closeReader(const std::shared_ptr<State>& state,std::exception_ptr primary){
  if(state->closing)return;
  state->closing=true;
  if(!state->reader){
    if(primary)reject(state,primary);else complete(state);return;
  }
  try{
    if(!state->reader->close)throw std::invalid_argument("History reader close import required");
    auto close=state->reader->close();
    close.then([state,primary](const event_loop::Unit*,const std::exception_ptr& error){
      if(error)reject(state,error);
      else if(primary)reject(state,primary);
      else complete(state);
    });
  }catch(...){reject(state,std::current_exception());}
}
void advance(const std::shared_ptr<State>& state){
  if(state->done||state->closing||!state->pending)return;
  try{
    if(!state->reader->next)throw std::invalid_argument("History reader next import required");
    auto next=state->reader->next();
    next.then([state](const std::optional<LogEntryRef>* item,
        const std::exception_ptr& error){
      if(error){reject(state,error);return;}
      if(!item->has_value()){complete(state);return;}
      try{
        if(!state->imports.logEntryToHistoryEntry)
          throw std::invalid_argument("History entry resolver import required");
        auto resolved=state->imports.logEntryToHistoryEntry(**item);
        resolved.then([state](const Json* value,const std::exception_ptr& failure){
          if(failure){closeReader(state,failure);return;}
          emit(state,*value);
        });
      }catch(...){closeReader(state,std::current_exception());}
    });
  }catch(...){reject(state,std::current_exception());}
}
}
HistorySimpleCursor::HistorySimpleCursor(HistoryViewImports imports):
    state_(std::make_shared<State>(std::move(imports))){
  if(!state_->imports.loop)throw std::invalid_argument("Owner loop required");
}
event_loop::Deferred<std::optional<Json>> HistorySimpleCursor::next(){
  auto state=state_;
  if(!state->imports.loop->isOwnerThread())throw std::logic_error("Owner required");
  event_loop::Deferred<std::optional<Json>> result(*state->imports.loop);
  if(state->pending)throw std::logic_error("Concurrent history next");
  if(state->done){result.resolve(std::nullopt);return result;}
  state->pending=result;
  if(!state->started){
    state->started=true;
    try{
      if(!state->imports.makeLogEntryReader)
        throw std::invalid_argument("History reader import required");
      state->reader=state->imports.makeLogEntryReader();
    }catch(...){reject(state,std::current_exception());return result;}
  }
  advance(state);return result;
}
event_loop::Deferred<event_loop::Unit> HistorySimpleCursor::close(){
  auto state=state_;
  if(!state->imports.loop->isOwnerThread())throw std::logic_error("Owner required");
  event_loop::Deferred<event_loop::Unit> result(*state->imports.loop);
  if(state->done){result.resolve({});return result;}
  if(state->closingResult)throw std::logic_error("History close already pending");
  state->closingResult=result;state->returnRequested=true;
  if(!state->pending)closeReader(state,{});
  return result;
}
}
