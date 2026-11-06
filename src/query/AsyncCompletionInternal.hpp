#pragma once
#include "query/AsyncCompletion.hpp"
#include <deque>

namespace batbox::query::async_completion {
struct Run:std::enable_shared_from_this<Run> {
  enum class Stage {Start,StopStart,StopAwait,AfterStop,Done};
  event_loop::Loop& loop;
  Params params;
  State state;
  std::shared_ptr<PreparedIteration> prepared;
  std::shared_ptr<ModelPassResult> modelResult;
  AsyncCompletionServices services;
  Stage stage=Stage::Start;
  std::deque<event_loop::Deferred<AsyncDecisionStep>> requests;
  std::optional<AsyncStopCursor> stopCursor;
  bool pausedAfterYield=false;
  Json stopResult;
  std::exception_ptr failure;
  std::optional<event_loop::Deferred<event_loop::Unit>> closing;
  Run(event_loop::Loop& owner,Params p,State s,
    std::shared_ptr<PreparedIteration> prep,
    std::shared_ptr<ModelPassResult> result,AsyncCompletionServices sv)
    :loop(owner),params(std::move(p)),state(std::move(s)),
     prepared(std::move(prep)),modelResult(std::move(result)),
     services(std::move(sv)){}
  event_loop::Deferred<AsyncDecisionStep> next();
  event_loop::Deferred<event_loop::Unit> close();
  void pump();
  void emit(const model::MessageRef& event,Stage after);
  void finish(CompletionDecision decision);
  void fail(std::exception_ptr error);
  void start();
  void startStopHooks();
  void pullStopHooks();
  void afterStopHooks();
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
