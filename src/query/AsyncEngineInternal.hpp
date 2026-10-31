#pragma once
#include "query/AsyncEngine.hpp"
#include <deque>

namespace batbox::query::async_engine {
struct Run:std::enable_shared_from_this<Run> {
  enum class Stage {Start,SetupStart,SetupAwait,ModelStart,ModelAwait,AfterModel,
    AbortStart,AbortAwait,AbortMissing,AbortCleanup,AbortCleanupAwait,
    AbortInterruption,AbortTerminal,SummaryStart,SummaryAwait,
    DecisionStart,DecisionAwait,ContextAwait,Done};
  event_loop::Loop& loop;
  Params params;
  std::vector<std::string>& consumedCommandUuids;
  AsyncEngineServices services;
  State state;
  QueryConfig config;
  std::optional<BudgetTracker> budgetTracker;
  std::optional<double> remaining;
  MemoryPrefetch memoryPrefetch;
  std::shared_ptr<PreparedIteration> prepared;
  std::shared_ptr<ModelPassResult> modelResult;
  std::optional<AsyncSetupIterator> setup;
  std::optional<AsyncModelPassIterator> model;
  std::optional<AsyncModelCursor> abortCursor;
  std::optional<AsyncDecisionCursor> decision;
  std::vector<Json> missingArgs;
  std::size_t missingIndex=0;
  Stage stage=Stage::Start;
  std::deque<event_loop::Deferred<AsyncEngineStep>> requests;
  bool pausedAfterYield=false;
  bool disposalAttempted=false;
  std::exception_ptr failure;
  std::optional<event_loop::Deferred<AsyncEngineStep>> closing;
  Run(event_loop::Loop& owner,Params p,std::vector<std::string>& consumed,
    AsyncEngineServices sv):loop(owner),params(std::move(p)),
    consumedCommandUuids(consumed),services(std::move(sv)){}
  event_loop::Deferred<AsyncEngineStep> next();
  event_loop::Deferred<AsyncEngineStep> returnEarly(const Json& value);
  event_loop::Deferred<AsyncEngineStep> throwEarly(std::exception_ptr error);
  void pump();
  void emit(const model::MessageRef& event,Stage after,
    model::MessageRef tombstoneTarget={});
  void finish(const Json& terminal);
  void fail(std::exception_ptr error);
  void dispose(std::exception_ptr prior={});
  void start();
  void startSetup();
  void pullSetup();
  void refreshUserContext(bool restartSetup);
  void startModel();
  void pullModel();
  void afterModel();
  void startAbort();
  void pullAbort();
  void emitMissing();
  void startAbortCleanup();
  void emitAbortInterruption();
  void startSummary();
  void startDecision();
  void pullDecision();
};
template<class T>void need(const T& callback,const char* name){
  if(!callback)throw std::invalid_argument(std::string("Unbound async query import: ")+name);
}
inline const Json& field(const Json& object,const char* key){return value::property(object,key);}
inline Json concatMessages(const Json& a,const Json& b){
  Json result=a.is_array()?a:Json::array();
  if(b.is_array())for(const Json& item:b)result.push_back(item);
  return result;
}
} // namespace batbox::query::async_engine
