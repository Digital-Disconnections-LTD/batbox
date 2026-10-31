#include "history/Views.hpp"
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace batbox::history {
struct TimestampedHistoryCursor::State {
  explicit State(HistoryViewImports value):imports(std::move(value)){}
  HistoryViewImports imports;
  std::optional<LogEntryCursor> reader;
  std::optional<event_loop::Deferred<std::optional<TimestampedHistoryEntry>>> pending;
  std::optional<event_loop::Deferred<event_loop::Unit>> closingResult;
  std::string currentProject;
  std::unordered_set<std::string> seen;
  bool started=false,done=false,closing=false,returnRequested=false;
  bool resumedAfterYield=false;
};
namespace {
using State=TimestampedHistoryCursor::State;
void closeReader(const std::shared_ptr<State>& state,std::exception_ptr primary);
void emit(const std::shared_ptr<State>& state,
    std::optional<TimestampedHistoryEntry> value){
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
  if(!state->reader){if(primary)reject(state,primary);else complete(state);return;}
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
      const auto entry=**item;
      if(!entry||!entry->is_object()||
          !value::property(*entry,"project").is_string()||
          value::property(*entry,"project")!=state->currentProject){
        advance(state);return;
      }
      const auto display=value::property(*entry,"display");
      const auto displayText=display.is_string()?display.get<std::string>():
        std::string{};
      if(state->seen.contains(displayText)){advance(state);return;}
      state->seen.insert(displayText);
      TimestampedHistoryEntry value;
      value.display=displayText;
      const auto timestamp=value::property(*entry,"timestamp");
      if(timestamp.is_number())value.timestamp=timestamp.get<double>();
      auto resolve=state->imports.logEntryToHistoryEntry;
      value.resolve=[resolve,entry]{
        if(!resolve)throw std::invalid_argument("History entry resolver import required");
        return resolve(entry);
      };
      state->resumedAfterYield=true;
      emit(state,std::move(value));
    });
  }catch(...){reject(state,std::current_exception());}
}
}
TimestampedHistoryCursor::TimestampedHistoryCursor(HistoryViewImports imports):
    state_(std::make_shared<State>(std::move(imports))){
  if(!state_->imports.loop)throw std::invalid_argument("Owner loop required");
}
event_loop::Deferred<std::optional<TimestampedHistoryEntry>>
    TimestampedHistoryCursor::next(){
  auto state=state_;
  if(!state->imports.loop->isOwnerThread())throw std::logic_error("Owner required");
  event_loop::Deferred<std::optional<TimestampedHistoryEntry>> result(*state->imports.loop);
  if(state->pending)throw std::logic_error("Concurrent history next");
  if(state->done){result.resolve(std::nullopt);return result;}
  state->pending=result;
  if(!state->started){
    state->started=true;
    try{
      if(!state->imports.getProjectRoot||!state->imports.makeLogEntryReader)
        throw std::invalid_argument("History view imports required");
      state->currentProject=state->imports.getProjectRoot();
      state->reader=state->imports.makeLogEntryReader();
    }catch(...){reject(state,std::current_exception());return result;}
  }
  if(state->resumedAfterYield&&state->seen.size()>=100){
    closeReader(state,{});return result;
  }
  state->resumedAfterYield=false;
  advance(state);return result;
}
event_loop::Deferred<event_loop::Unit> TimestampedHistoryCursor::close(){
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
