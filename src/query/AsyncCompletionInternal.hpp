#pragma once
#include "query/AsyncCompletion.hpp"
#include <deque>

namespace batbox::query::async_completion {
struct Run:std::enable_shared_from_this<Run> {
  enum class Stage {Start,AwaitReactive,PostCompact,AfterPostCompact,
    AfterWithheldError,AfterExhaustedMax,StopStart,StopAwait,
    AfterStop,Budget,Done};
  event_loop::Loop& loop;
  Params params;
  State state;
  std::shared_ptr<PreparedIteration> prepared;
  std::shared_ptr<ModelPassResult> modelResult;
  std::optional<double>& remaining;
  BudgetTracker* budgetTracker;
  AsyncCompletionServices services;
  Stage stage=Stage::Start;
  std::deque<event_loop::Deferred<AsyncDecisionStep>> requests;
  std::optional<AsyncStopCursor> stopCursor;
  Json postMessages=Json::array();
  std::size_t postIndex=0;
  bool withheldMedia=false,withheld413=false;
  bool pausedAfterYield=false;
  Json stopResult;
  std::exception_ptr failure;
  std::optional<event_loop::Deferred<event_loop::Unit>> closing;
  Run(event_loop::Loop& owner,Params p,State s,
    std::shared_ptr<PreparedIteration> prep,
    std::shared_ptr<ModelPassResult> result,std::optional<double>& budget,
    BudgetTracker* tracker,AsyncCompletionServices sv)
    :loop(owner),params(std::move(p)),state(std::move(s)),
     prepared(std::move(prep)),modelResult(std::move(result)),remaining(budget),
     budgetTracker(tracker),services(std::move(sv)){}
  event_loop::Deferred<AsyncDecisionStep> next();
  event_loop::Deferred<event_loop::Unit> close();
  void pump();
  void emit(const model::MessageRef& event,Stage after);
  void finish(CompletionDecision decision);
  void fail(std::exception_ptr error);
  void start();
  void startReactive();
  void afterReactive(const Json& compacted);
  void emitPostCompact();
  void finishReactive();
  void afterWithheldError();
  void afterExhaustedMax();
  void startStopHooks();
  void pullStopHooks();
  void afterStopHooks();
  void checkBudget();
  const Json& lastMessage()const;
  model::MessageRef lastMessageRef()const;
  State nextState()const;
};
template<class T>void need(const T& callback,const char* name){
  if(!callback)throw std::invalid_argument(std::string("Unbound async completion import: ")+name);
}
inline const Json& field(const Json& object,const char* key){return value::property(object,key);}
inline Json appendMessageArrays(const Json& a,const Json& b){
  Json out=a.is_array()?a:Json::array();
  if(b.is_array())for(const Json& item:b)out.push_back(item);
  return out;
}
} // namespace batbox::query::async_completion
