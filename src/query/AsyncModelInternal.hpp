#pragma once
#include "query/AsyncModelPass.hpp"
#include <deque>
#include <memory>
#include <stdexcept>

namespace batbox::query::async_model {
struct Run:std::enable_shared_from_this<Run> {
  enum class Stage {Start,StartAttempt,AwaitMessage,InjectAwait,HandleMessage,AfterTombstones,
    AfterAssistantYield,EmitGadgetResult,AfterGadgetResultYield,AfterStream,
    EmitMissing,AfterMissing,EmitFallbackSystem,AfterFallbackSystem,
    EmitFailureMessage,AfterFailureMessage,AfterStashedBoundary,Done};
  event_loop::Loop& loop;
  Params params;
  State state;
  std::shared_ptr<PreparedIteration> prepared;
  QueryConfig config;
  std::optional<double> remaining;
  AsyncModelServices services;
  ModelPassResult result;
  Stage stage=Stage::Start;
  AsyncModelCursor cursor;
  std::deque<event_loop::Deferred<AsyncModelStep>> requests;
  std::atomic_bool streamingFallbackOccurred=false;
  std::optional<model::MessageRef> currentMessage;
  model::MessageRef currentEmission;
  std::vector<model::MessageRef> tombstones;
  std::size_t tombstoneIndex=0;
  std::vector<execution::Update> completedGadgets;
  std::size_t completedIndex=0;
  std::optional<Json> normalizedGadgetMessage;
  std::vector<Json> missingArgs;
  std::size_t missingIndex=0;
  std::string errorMessage;
  std::optional<std::pair<std::string,std::string>> fallbackNames;
  std::exception_ptr failure;
  std::optional<event_loop::Deferred<event_loop::Unit>> closing;
  bool pausedAfterYield=false;
  bool handlingError=false;
  Run(event_loop::Loop& owner,Params p,State s,
      std::shared_ptr<PreparedIteration> current,QueryConfig c,
      std::optional<double> budget,AsyncModelServices sv)
    :loop(owner),params(std::move(p)),state(std::move(s)),prepared(std::move(current)),
     config(std::move(c)),remaining(budget),services(std::move(sv)){}
  event_loop::Deferred<AsyncModelStep> next();
  event_loop::Deferred<event_loop::Unit> close();
  event_loop::Deferred<AsyncModelStep> throwEarly(std::exception_ptr error);
  void pump();
  void emit(const model::MessageRef& message,Stage after,
    model::MessageRef tombstoneTarget={});
  void finish();
  void fail(std::exception_ptr error);
  void startAttempt();
  void requestMessage();
  void onMessage(const std::optional<model::MessageRef>& message);
  void handleMessage();
  void afterTombstones();
  void afterAssistantYield();
  void processGadgetResults();
  void afterGadgetResultYield();
  void afterStream();
  void onStreamError(std::exception_ptr error,bool allowFallback=true);
  void emitMissing();
  void afterMissing();
  void emitFallbackSystem();
  void afterFallbackSystem();
  void emitFailureMessage();
  void afterFailureMessage();
  void syncAssistantValues();
};
template<class T>void need(const T& callback,const char* name){
  if(!callback)throw std::invalid_argument(std::string("Unbound async model import: ")+name);
}
inline const Json& field(const Json& object,const char* key){return value::property(object,key);}
inline std::string stringField(const Json& object,const char* key){
  const auto& value=field(object,key);return value.is_string()?value.get<std::string>():std::string{};
}
} // namespace batbox::query::async_model
