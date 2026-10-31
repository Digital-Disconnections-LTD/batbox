#pragma once
#include "event_loop/Deferred.hpp"
#include <algorithm>
#include <cmath>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace batbox::instructions::hooks::runtime {
template<class T>struct GeneratorStep {
  bool done=false;
  std::optional<T> value;
};
template<class T>using GeneratorNext=
  std::function<event_loop::Deferred<GeneratorStep<T>>() >;

template<class T>class ConcurrentAll {
  struct Queued {
    std::size_t id=0,generator=0;
    GeneratorStep<T> step;
  };
  struct Pending {
    std::size_t id=0;
    event_loop::Deferred<Queued> promise;
  };
  struct Demand {
    enum class Kind { Next,Return,Throw } kind=Kind::Next;
    event_loop::Deferred<GeneratorStep<T>> promise;
    std::optional<T> value;
    std::exception_ptr error;
  };
  struct State:std::enable_shared_from_this<State>{
    event_loop::Loop& loop;
    std::vector<GeneratorNext<T>> generators;
    std::deque<std::size_t> waiting;
    std::vector<Pending> promises;
    std::deque<Demand> demands;
    double concurrencyCap;
    std::size_t nextId=1;
    bool started=false,closed=false,waitingForRace=false;
    State(event_loop::Loop& owner,std::vector<GeneratorNext<T>> values,
      double cap):loop(owner),generators(std::move(values)),concurrencyCap(cap){
      for(std::size_t index=0;index<generators.size();++index)
        waiting.push_back(index);
    }
    void startNext(std::size_t generator){
      if(!generators.at(generator))
        throw std::invalid_argument("Generator next import required");
      auto next=generators[generator]();
      event_loop::Deferred<Queued> wrapped(loop);
      const auto id=nextId++;
      // An outer iterator return closes all() but does not cancel child next() calls
      // already in flight. Retain their generator owners until each pending Promise
      // settles.
      const auto keepAlive=this->shared_from_this();
      next.then([keepAlive,wrapped,id,generator](const GeneratorStep<T>* step,
        const std::exception_ptr& failure){
        (void)keepAlive;
        if(failure)wrapped.reject(failure);
        else wrapped.resolve(Queued{id,generator,*step});
      });
      promises.push_back(Pending{id,std::move(wrapped)});
    }
    void startBatch(){
      while(static_cast<double>(promises.size())<concurrencyCap&&
        !waiting.empty()){
        const auto generator=waiting.front();waiting.pop_front();
        startNext(generator);
      }
    }
    void done(){
      closed=true;
      while(!demands.empty()){
        auto demand=std::move(demands.front());demands.pop_front();
        if(demand.kind==Demand::Kind::Throw)demand.promise.reject(demand.error);
        else demand.promise.resolve(GeneratorStep<T>{true,demand.value});
      }
    }
    void reject(std::exception_ptr failure){
      closed=true;
      if(!demands.empty()){
        auto demand=std::move(demands.front());demands.pop_front();
        demand.promise.reject(failure);
      }
      done();
    }
    void pump(){
      if(closed||waitingForRace||demands.empty())return;
      try{
        if(demands.front().kind!=Demand::Kind::Next){
          auto demand=std::move(demands.front());demands.pop_front();
          if(demand.kind==Demand::Kind::Return)
            demand.promise.resolve(GeneratorStep<T>{true,demand.value});
          else demand.promise.reject(demand.error);
          done();return;
        }
        if(!started){started=true;startBatch();}
        if(promises.empty()){done();return;}
        std::vector<event_loop::Deferred<Queued>> options;
        options.reserve(promises.size());
        for(const auto& pending:promises)options.push_back(pending.promise);
        waitingForRace=true;
        const auto weak=this->weak_from_this();
        event_loop::race<Queued>(loop,options).then(
          [weak](const Queued* winner,const std::exception_ptr& failure){
            if(auto current=weak.lock())current->onRace(winner,failure);
          });
      }catch(...){reject(std::current_exception());}
    }
    void onRace(const Queued* winner,const std::exception_ptr& failure){
      if(closed)return;
      waitingForRace=false;
      if(failure){reject(failure);return;}
      try{
        const auto it=std::find_if(promises.begin(),promises.end(),
          [winner](const Pending& row){return row.id==winner->id;});
        if(it==promises.end())throw std::logic_error("Generator race winner missing");
        promises.erase(it);
        if(!winner->step.done){
          startNext(winner->generator);
          if(winner->step.value){
            auto demand=std::move(demands.front());demands.pop_front();
            demand.promise.resolve(GeneratorStep<T>{false,winner->step.value});
            pump();return;
          }
        }else if(!waiting.empty()){
          const auto generator=waiting.front();waiting.pop_front();
          startNext(generator);
        }
        pump();
      }catch(...){reject(std::current_exception());}
    }
  };
public:
  ConcurrentAll(event_loop::Loop& loop,std::vector<GeneratorNext<T>> generators,
    double concurrencyCap=std::numeric_limits<double>::infinity()):
    state_(std::make_shared<State>(loop,std::move(generators),concurrencyCap)){}
  event_loop::Deferred<GeneratorStep<T>> next(){
    auto& state=*state_;
    if(!state.loop.isOwnerThread())
      throw std::logic_error("Concurrent generator owner required");
    event_loop::Deferred<GeneratorStep<T>> output(state.loop);
    if(state.closed)output.resolve(GeneratorStep<T>{true,std::nullopt});
    else{state.demands.push_back(Demand{Demand::Kind::Next,output,{},{}});
      state.pump();}
    return output;
  }
  event_loop::Deferred<GeneratorStep<T>> returnValue(std::optional<T> value={}){
    auto& state=*state_;
    if(!state.loop.isOwnerThread())
      throw std::logic_error("Concurrent generator owner required");
    event_loop::Deferred<GeneratorStep<T>> output(state.loop);
    if(state.closed)output.resolve(GeneratorStep<T>{true,std::move(value)});
    else{state.demands.push_back(Demand{Demand::Kind::Return,output,
      std::move(value),{}});state.pump();}
    return output;
  }
  event_loop::Deferred<GeneratorStep<T>> throwError(std::exception_ptr error){
    if(!error)throw std::invalid_argument("Generator throw error required");
    auto& state=*state_;
    if(!state.loop.isOwnerThread())
      throw std::logic_error("Concurrent generator owner required");
    event_loop::Deferred<GeneratorStep<T>> output(state.loop);
    if(state.closed)output.reject(error);
    else{state.demands.push_back(Demand{Demand::Kind::Throw,output,{},error});
      state.pump();}
    return output;
  }
private:
  std::shared_ptr<State> state_;
};
} // namespace batbox::instructions::hooks::runtime
