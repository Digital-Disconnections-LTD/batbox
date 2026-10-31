#include "history/Reader.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::history {
struct HistoryLogReader::State {
  State(std::shared_ptr<HistoryState> value,HistoryReaderImports providers):
    shared(std::move(value)),imports(std::move(providers)){}
  std::shared_ptr<HistoryState> shared;
  HistoryReaderImports imports;
  std::string currentFlight;
  std::optional<HistoryLineCursor> lines;
  std::optional<event_loop::Deferred<std::optional<LogEntryRef>>> pending;
  std::optional<event_loop::Deferred<event_loop::Unit>> closingResult;
  std::ptrdiff_t pendingIndex=-1;
  bool started=false,done=false,closing=false,returnRequested=false;
  bool classifyOuterError=false;
};
namespace {
using State=HistoryLogReader::State;
template<class T>void required(const T& callback){
  if(!callback)throw std::invalid_argument("History reader import is required");
}
void closeCursor(const std::shared_ptr<State>& state,std::exception_ptr primary);
void resolvePending(const std::shared_ptr<State>& state,
    std::optional<LogEntryRef> value){
  auto pending=std::move(state->pending);state->pending.reset();
  if(pending)pending->resolve(std::move(value));
  if(pending&&state->returnRequested&&!state->closing&&!state->done)
    closeCursor(state,{});
}
void rejectPending(const std::shared_ptr<State>& state,std::exception_ptr error){
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
  state->done=true;
  resolvePending(state,std::nullopt);
  if(state->closingResult){state->closingResult->resolve({});
    state->closingResult.reset();}
}
void outerError(const std::shared_ptr<State>& state,std::exception_ptr error){
  try{
    required(state->imports.getErrnoCode);
    if(state->imports.getErrnoCode(error)=="ENOENT"){complete(state);return;}
  }catch(...){rejectPending(state,std::current_exception());return;}
  rejectPending(state,error);
}
void closeCursor(const std::shared_ptr<State>& state,std::exception_ptr primary){
  if(state->closing)return;
  state->closing=true;
  if(!state->lines){
    if(primary){
      if(state->classifyOuterError)outerError(state,primary);
      else rejectPending(state,primary);
    }else complete(state);
    return;
  }
  try{
    required(state->lines->close);
    auto close=state->lines->close();
    close.then([state,primary](const event_loop::Unit*,
        const std::exception_ptr& closeError){
      if(closeError){
        if(state->classifyOuterError)outerError(state,closeError);
        else rejectPending(state,closeError);
        return;
      }
      if(primary){
        if(state->classifyOuterError)outerError(state,primary);
        else rejectPending(state,primary);
        return;
      }
      complete(state);
    });
  }catch(...){
    if(state->classifyOuterError)outerError(state,std::current_exception());
    else rejectPending(state,std::current_exception());
  }
}
void readLine(const std::shared_ptr<State>& state){
  if(state->done||state->closing||!state->pending)return;
  try{
    required(state->lines->next);
    auto next=state->lines->next();
    next.then([state](const std::optional<std::string>* line,
        const std::exception_ptr& error){
      if(error){
        try{
          required(state->imports.getErrnoCode);
          if(state->imports.getErrnoCode(error)=="ENOENT"){
            complete(state);return;
          }
        }catch(...){rejectPending(state,std::current_exception());return;}
        rejectPending(state,error);return;
      }
      if(!line->has_value()){complete(state);return;}
      try{
        required(state->imports.jsonParse);
        auto entry=std::make_shared<Json>(state->imports.jsonParse(**line));
        const auto& flight=value::property(*entry,"sessionId");
        const auto& timestamp=value::property(*entry,"timestamp");
        if(flight.is_string()&&flight.get<std::string>()==state->currentFlight&&
            timestamp.is_number()&&
            state->shared->skippedTimestamps.contains(timestamp.get<double>())){
          readLine(state);return;
        }
        resolvePending(state,entry);
      }catch(...){
        try{
          required(state->imports.logForDebugging);
          required(state->imports.errorToString);
          state->imports.logForDebugging("Failed to parse history line: "+
            state->imports.errorToString(std::current_exception()));
          readLine(state);
        }catch(...){
          state->classifyOuterError=true;
          closeCursor(state,std::current_exception());
        }
      }
    });
  }catch(...){rejectPending(state,std::current_exception());}
}
void advance(const std::shared_ptr<State>& state){
  if(state->done||state->closing||!state->pending)return;
  if(state->pendingIndex>=0){
    LogEntryRef entry;
    const auto index=static_cast<std::size_t>(state->pendingIndex--);
    if(index<state->shared->pendingEntries.size())
      entry=state->shared->pendingEntries[index];
    resolvePending(state,entry);return;
  }
  if(state->lines){readLine(state);return;}
  try{
    required(state->imports.getConfigHomeDir);required(state->imports.join);
    required(state->imports.readLinesReverse);
    const auto path=state->imports.join(state->imports.getConfigHomeDir(),
      "history.jsonl");
    state->lines=state->imports.readLinesReverse(path);
    readLine(state);
  }catch(...){rejectPending(state,std::current_exception());}
}
}
HistoryLogReader::HistoryLogReader(std::shared_ptr<HistoryState> shared,
    HistoryReaderImports imports):
    state_(std::make_shared<State>(std::move(shared),std::move(imports))){
  if(!state_->shared||!state_->imports.loop)
    throw std::invalid_argument("History state and owner loop are required");
}
event_loop::Deferred<std::optional<LogEntryRef>> HistoryLogReader::next(){
  auto state=state_;
  if(!state->imports.loop->isOwnerThread())
    throw std::logic_error("History reader next requires loop owner");
  event_loop::Deferred<std::optional<LogEntryRef>> result(*state->imports.loop);
  if(state->pending)throw std::logic_error("Concurrent history reader next");
  if(state->done){result.resolve(std::nullopt);return result;}
  state->pending=result;
  if(!state->started){
    state->started=true;
    try{
      required(state->imports.getFlightId);
      state->currentFlight=state->imports.getFlightId();
      state->pendingIndex=static_cast<std::ptrdiff_t>(
        state->shared->pendingEntries.size())-1;
    }catch(...){rejectPending(state,std::current_exception());return result;}
  }
  advance(state);return result;
}
event_loop::Deferred<event_loop::Unit> HistoryLogReader::close(){
  auto state=state_;
  if(!state->imports.loop->isOwnerThread())
    throw std::logic_error("History reader close requires loop owner");
  event_loop::Deferred<event_loop::Unit> result(*state->imports.loop);
  if(state->done){result.resolve({});return result;}
  if(state->closingResult)throw std::logic_error("History reader close already pending");
  state->closingResult=result;state->returnRequested=true;
  if(state->pending)return result;
  closeCursor(state,{});return result;
}
}
