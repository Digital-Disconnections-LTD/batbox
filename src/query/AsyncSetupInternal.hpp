#pragma once
#include "query/AsyncSetup.hpp"
#include <deque>
#include <exception>
#include <memory>
#include <stdexcept>

namespace batbox::query::async_setup {
struct Run:std::enable_shared_from_this<Run> {
  enum class Stage {Start,AfterStart,AwaitBudget,AfterBudget,
    AwaitMicro,AfterMicro,AwaitAuto,YieldPost,AfterPost,AfterBlocking,Done};
  event_loop::Loop& loop;
  Params params;
  State state;
  AsyncSetupServices services;
  std::optional<double> remaining;
  PreparedIteration out;
  Stage stage=Stage::Start;
  std::deque<event_loop::Deferred<AsyncSetupStep>> requests;
  Json postMessages=Json::array();
  std::size_t postIndex=0;
  bool pausedAfterYield=false;
  std::exception_ptr failure;
  Run(event_loop::Loop& owner,Params p,State s,AsyncSetupServices sv,
      std::optional<double> budget)
    :loop(owner),params(std::move(p)),state(std::move(s)),
     services(std::move(sv)),remaining(budget){}
  event_loop::Deferred<AsyncSetupStep> next();
  void pump();
  void emit(const Json& event,Stage after);
  void finish();
  void fail(std::exception_ptr error);
  void start();
  void startBudget();
  void startMicrocompact();
  void afterMicrocompact(const Json& micro);
  void startAutocompact();
  void afterAutocompact(const Json& compact);
  void finishSetup();
};
template<class T>void need(const T& callback,const char* name){
  if(!callback)throw std::invalid_argument(std::string("Unbound async query import: ")+name);
}
inline const Json& field(const Json& object,const char* key){return value::property(object,key);}
} // namespace batbox::query::async_setup
