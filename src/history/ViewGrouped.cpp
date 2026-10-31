#include "history/Views.hpp"
#include <stdexcept>
#include <utility>
#include <vector>

namespace batbox::history {
struct GroupedHistoryCursor::State {
  explicit State(HistoryViewImports value):imports(std::move(value)){}
  enum class Phase{Scan,Other,Done};
  HistoryViewImports imports;
  std::optional<LogEntryCursor> reader;
  std::optional<event_loop::Deferred<std::optional<Json>>> pending;
  std::optional<event_loop::Deferred<event_loop::Unit>> closingResult;
  std::string currentProject,currentFlight;
  std::vector<LogEntryRef> otherFlightEntries;
  std::size_t otherIndex=0,yielded=0;
  Phase phase=Phase::Scan;
  bool started=false,done=false,closing=false,returnRequested=false;
  bool afterCurrentYield=false,afterOtherYield=false;
};
namespace {
using State=GroupedHistoryCursor::State;
void closeReader(const std::shared_ptr<State>& state,std::exception_ptr primary);
void advanceOther(const std::shared_ptr<State>& state);
void emit(const std::shared_ptr<State>& state,std::optional<Json> value){
  auto pending=std::move(state->pending);state->pending.reset();
  if(pending)pending->resolve(std::move(value));
  if(pending&&state->returnRequested&&!state->closing&&!state->done)
    closeReader(state,{});
}
void reject(const std::shared_ptr<State>& state,std::exception_ptr error){
  state->done=true;state->phase=State::Phase::Done;
  auto pending=std::move(state->pending);state->pending.reset();
  if(pending)pending->reject(error);
  if(state->closingResult){
    if(pending&&state->returnRequested)state->closingResult->resolve({});
    else state->closingResult->reject(error);
    state->closingResult.reset();
  }
}
void complete(const std::shared_ptr<State>& state){
  state->done=true;state->phase=State::Phase::Done;emit(state,std::nullopt);
  if(state->closingResult){state->closingResult->resolve({});
    state->closingResult.reset();}
}
void closeReader(const std::shared_ptr<State>& state,std::exception_ptr primary){
  if(state->closing)return;
  state->closing=true;
  if(!state->reader||state->phase!=State::Phase::Scan){
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
void resolveEntry(const std::shared_ptr<State>& state,const LogEntryRef& entry,
    bool current){
  try{
    if(!state->imports.logEntryToHistoryEntry)
      throw std::invalid_argument("History entry resolver import required");
    auto result=state->imports.logEntryToHistoryEntry(entry);
    result.then([state,current](const Json* value,const std::exception_ptr& error){
      if(error){closeReader(state,error);return;}
      if(current)state->afterCurrentYield=true;
      else state->afterOtherYield=true;
      emit(state,*value);
    });
  }catch(...){closeReader(state,std::current_exception());}
}
void closeForWindow(const std::shared_ptr<State>& state){
  try{
    if(!state->reader->close)throw std::invalid_argument("History reader close import required");
    auto close=state->reader->close();
    close.then([state](const event_loop::Unit*,const std::exception_ptr& error){
      if(error){reject(state,error);return;}
      state->phase=State::Phase::Other;
      advanceOther(state);
    });
  }catch(...){reject(state,std::current_exception());}
}
void advanceScan(const std::shared_ptr<State>& state){
  if(state->done||state->closing||!state->pending)return;
  try{
    if(!state->reader->next)throw std::invalid_argument("History reader next import required");
    auto next=state->reader->next();
    next.then([state](const std::optional<LogEntryRef>* item,
        const std::exception_ptr& error){
      if(error){reject(state,error);return;}
      if(!item->has_value()){
        state->phase=State::Phase::Other;advanceOther(state);return;
      }
      const auto entry=**item;
      if(!entry||!entry->is_object()||
          !value::property(*entry,"project").is_string()||
          value::property(*entry,"project")!=state->currentProject){
        advanceScan(state);return;
      }
      if(value::property(*entry,"sessionId").is_string()&&
          value::property(*entry,"sessionId")==state->currentFlight){
        resolveEntry(state,entry,true);return;
      }
      state->otherFlightEntries.push_back(entry);
      if(state->yielded+state->otherFlightEntries.size()>=100){
        closeForWindow(state);return;
      }
      advanceScan(state);
    });
  }catch(...){reject(state,std::current_exception());}
}
void advanceOther(const std::shared_ptr<State>& state){
  if(state->done||state->closing||!state->pending)return;
  if(state->yielded>=100||state->otherIndex>=state->otherFlightEntries.size()){
    complete(state);return;
  }
  const auto entry=state->otherFlightEntries[state->otherIndex++];
  resolveEntry(state,entry,false);
}
}
GroupedHistoryCursor::GroupedHistoryCursor(HistoryViewImports imports):
    state_(std::make_shared<State>(std::move(imports))){
  if(!state_->imports.loop)throw std::invalid_argument("Owner loop required");
}
event_loop::Deferred<std::optional<Json>> GroupedHistoryCursor::next(){
  auto state=state_;
  if(!state->imports.loop->isOwnerThread())throw std::logic_error("Owner required");
  event_loop::Deferred<std::optional<Json>> result(*state->imports.loop);
  if(state->pending)throw std::logic_error("Concurrent history next");
  if(state->done){result.resolve(std::nullopt);return result;}
  state->pending=result;
  if(!state->started){
    state->started=true;
    try{
      if(!state->imports.getProjectRoot||!state->imports.getFlightId||
          !state->imports.makeLogEntryReader)
        throw std::invalid_argument("History view imports required");
      state->currentProject=state->imports.getProjectRoot();
      state->currentFlight=state->imports.getFlightId();
      state->reader=state->imports.makeLogEntryReader();
    }catch(...){reject(state,std::current_exception());return result;}
  }
  if(state->afterCurrentYield){
    state->afterCurrentYield=false;++state->yielded;
    if(state->yielded+state->otherFlightEntries.size()>=100){
      closeForWindow(state);return result;
    }
  }
  if(state->afterOtherYield){state->afterOtherYield=false;++state->yielded;}
  if(state->phase==State::Phase::Scan)advanceScan(state);
  else advanceOther(state);
  return result;
}
event_loop::Deferred<event_loop::Unit> GroupedHistoryCursor::close(){
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
