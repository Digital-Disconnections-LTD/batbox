#include "instructions/hooks/runtime/ExecuteHooks.hpp"
#include <deque>
#include <stdexcept>

namespace batbox::instructions::hooks::runtime {
struct ExecuteHooks::State:std::enable_shared_from_this<State>{
  struct Demand {
    enum class Kind { Next,Return,Throw } kind=Kind::Next;
    event_loop::Deferred<HookYield> output;
    std::optional<Json> value;
    std::exception_ptr error;
  };
  enum class Stage { Start,Results,Closed } stage=Stage::Start;
  event_loop::Loop& loop;
  ExecuteHooksInput input;
  ExecuteHooksServices services;
  HookBatchStart batch;
  std::shared_ptr<ConcurrentAll<HookExecutionResult>> concurrent;
  std::unique_ptr<HookResultLoop> results;
  std::deque<Demand> demands;
  bool waiting=false;
  State(event_loop::Loop& owner,ExecuteHooksInput call,
    ExecuteHooksServices imports):loop(owner),input(std::move(call)),
    services(std::move(imports)),batch(loop,input.batch,services.batch){}
  void done(){
    stage=Stage::Closed;
    while(!demands.empty()){
      auto demand=std::move(demands.front());demands.pop_front();
      if(demand.kind==Demand::Kind::Throw)demand.output.reject(demand.error);
      else demand.output.resolve(HookYield{true,
        demand.value.value_or(Json(nullptr)),
        demand.kind==Demand::Kind::Next||
        (demand.kind==Demand::Kind::Return&&!demand.value)
          ?std::vector<std::string>{"value"}:std::vector<std::string>{}});
    }
  }
  void fail(std::exception_ptr failure){
    stage=Stage::Closed;
    if(!demands.empty()){
      auto next=std::move(demands.front());demands.pop_front();
      next.output.reject(failure);
    }
    done();
  }
  void yield(HookYield value){
    auto next=std::move(demands.front());demands.pop_front();
    next.output.resolve(std::move(value));
    if(!demands.empty()){
      const auto self=shared_from_this();
      loop.queueMicrotask([self]{self->pump();});
    }
  }
  void setup(const HookBatchStartStep& ready){
    if(!input.timeoutMs&&!services.defaultTimeoutMs)
      throw std::invalid_argument("Hook default timeout import required");
    const auto timeout=input.timeoutMs?*input.timeoutMs:
      services.defaultTimeoutMs();
    if(!services.jsonStringify)
      throw std::invalid_argument("Hook JSON stringify import required");
    if(!services.logError)
      throw std::invalid_argument("Hook local error logger required");
    auto perHook=services.perHook;
    perHook.jsonInput=std::make_shared<LazyHookJsonInput>(
      input.batch.hookInput,ready.hookName,services.jsonStringify,
      services.logError);
    std::vector<GeneratorNext<HookExecutionResult>> generators;
    generators.reserve(ready.matches.size());
    for(std::size_t index=0;index<ready.matches.size();++index){
      HookPerHookInput call;
      call.match=ready.matches[index];call.hookIndex=index;
      call.hookInput=input.batch.hookInput;
      call.hookName=ready.hookName;call.hookEvent=ready.hookEvent;
      call.gadgetUseID=input.batch.gadgetUseID;
      call.messages=input.messages;call.signal=input.batch.signal;
      call.gadgetUseContext=input.batch.gadgetUseContext;
      call.timeoutMs=timeout;
      call.forceSyncExecution=input.forceSyncExecution;
      call.boundRequestPoetry=ready.boundRequestPoetry;
      generators.push_back(makeHookPerHookGenerator(loop,std::move(call),
        perHook));
    }
    concurrent=std::make_shared<ConcurrentAll<HookExecutionResult>>(
      loop,std::move(generators));
    HookResultLoopInput resultInput;
    resultInput.hookEvent=ready.hookEvent;
    resultInput.hookName=ready.hookName;
    resultInput.gadgetUseID=input.batch.gadgetUseID;
    resultInput.matchQuery=input.batch.matchQuery;
    resultInput.appState=ready.appState;
    resultInput.batchStartTime=ready.batchStartTime;
    for(const auto& match:ready.matches)
      resultInput.matchingHooks.push_back({match.hook,match.hookSource});
    auto resultServices=services.results;
    const auto all=concurrent;
    resultServices.nextResult=[all]{return all->next();};
    results=std::make_unique<HookResultLoop>(loop,std::move(resultInput),
      std::move(resultServices));
    stage=Stage::Results;
  }
  void onStart(const HookBatchStartStep* value,
    const std::exception_ptr& failure){
    waiting=false;
    if(failure){fail(failure);return;}
    if(value->kind==HookBatchStartStep::Kind::Done){done();return;}
    if(value->kind==HookBatchStartStep::Kind::Progress){
      yield(HookYield{false,value->progress,value->ownUndefinedKeys});
      return;
    }
    try{setup(*value);pump();}
    catch(...){fail(std::current_exception());}
  }
  void onResult(const HookYield* value,const std::exception_ptr& failure){
    waiting=false;
    if(failure){fail(failure);return;}
    if(value->done){done();return;}
    yield(*value);
  }
  void pump(){
    if(waiting||demands.empty()||stage==Stage::Closed)return;
    if(demands.front().kind!=Demand::Kind::Next){
      auto demand=std::move(demands.front());demands.pop_front();
      if(stage!=Stage::Results||!concurrent){
        if(demand.kind==Demand::Kind::Throw)demand.output.reject(demand.error);
        else demand.output.resolve(HookYield{true,
          demand.value.value_or(Json(nullptr)),
          demand.value?std::vector<std::string>{}:
            std::vector<std::string>{"value"}});
        done();return;
      }
      waiting=true;
      const auto self=shared_from_this();
      try{
        concurrent->returnValue().then([self,demand](
          const GeneratorStep<HookExecutionResult>*,
          const std::exception_ptr& failure){
          self->waiting=false;
          if(failure){demand.output.reject(failure);self->done();return;}
          if(demand.kind==Demand::Kind::Throw)
            demand.output.reject(demand.error);
          else demand.output.resolve(HookYield{true,
            demand.value.value_or(Json(nullptr)),
            demand.value?std::vector<std::string>{}:
              std::vector<std::string>{"value"}});
          self->done();
        });
      }catch(...){waiting=false;demand.output.reject(std::current_exception());
        done();}
      return;
    }
    waiting=true;
    try{
      const auto self=shared_from_this();
      if(stage==Stage::Start){
        batch.next().then([self](const HookBatchStartStep* value,
          const std::exception_ptr& failure){
          self->onStart(value,failure);
        });
      }else{
        results->next().then([self](const HookYield* value,
          const std::exception_ptr& failure){
          self->onResult(value,failure);
        });
      }
    }catch(...){waiting=false;fail(std::current_exception());}
  }
};
ExecuteHooks::ExecuteHooks(event_loop::Loop& loop,ExecuteHooksInput input,
  ExecuteHooksServices services):state_(std::make_shared<State>(loop,
    std::move(input),std::move(services))){}
event_loop::Deferred<HookYield> ExecuteHooks::next(){
  auto& state=*state_;
  if(!state.loop.isOwnerThread())
    throw std::logic_error("Hook executor owner required");
  event_loop::Deferred<HookYield> output(state.loop);
  if(state.stage==State::Stage::Closed)
    output.resolve(HookYield{true,Json(nullptr),{"value"}});
  else{state.demands.push_back(State::Demand{
    State::Demand::Kind::Next,output,{},{}});state.pump();}
  return output;
}
event_loop::Deferred<HookYield> ExecuteHooks::returnValue(
  std::optional<Json> value){
  auto& state=*state_;
  if(!state.loop.isOwnerThread())
    throw std::logic_error("Hook executor owner required");
  event_loop::Deferred<HookYield> output(state.loop);
  if(state.stage==State::Stage::Closed)
    output.resolve(HookYield{true,value.value_or(Json(nullptr)),
      value?std::vector<std::string>{}:
        std::vector<std::string>{"value"}});
  else{state.demands.push_back(State::Demand{
    State::Demand::Kind::Return,output,std::move(value),{}});
    state.pump();}
  return output;
}
event_loop::Deferred<HookYield> ExecuteHooks::throwError(
  std::exception_ptr error){
  if(!error)throw std::invalid_argument("Hook generator throw error required");
  auto& state=*state_;
  if(!state.loop.isOwnerThread())
    throw std::logic_error("Hook executor owner required");
  event_loop::Deferred<HookYield> output(state.loop);
  if(state.stage==State::Stage::Closed)output.reject(error);
  else{state.demands.push_back(State::Demand{
    State::Demand::Kind::Throw,output,{},error});state.pump();}
  return output;
}
} // namespace batbox::instructions::hooks::runtime
