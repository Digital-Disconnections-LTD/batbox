#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace batbox::bootstrap {
template<class... Args>class Signal {
  struct State {
    using Listener=std::function<void(const Args&...)>;
    std::map<std::uint64_t,std::shared_ptr<Listener>> listeners;
    std::unordered_map<const Listener*,std::uint64_t> identities;
    std::uint64_t nextId=1;
  };
  std::shared_ptr<State> state_=std::make_shared<State>();
public:
  using Listener=std::function<void(const Args&...)>;
  std::function<void()> subscribe(std::shared_ptr<Listener> listener){
    if(!listener||!*listener)
      throw std::invalid_argument("Signal listener required");
    auto existing=state_->identities.find(listener.get());
    const auto id=existing!=state_->identities.end()?
      existing->second:state_->nextId++;
    if(existing==state_->identities.end()){
      state_->listeners.emplace(id,listener);
      state_->identities.emplace(listener.get(),id);
    }
    const std::weak_ptr<State> weak=state_;
    return [weak,listener]{if(auto state=weak.lock()){
      const auto identity=state->identities.find(listener.get());
      if(identity==state->identities.end())return;
      state->listeners.erase(identity->second);
      state->identities.erase(identity);
    }};
  }
  std::function<void()> subscribe(Listener listener){
    return subscribe(std::make_shared<Listener>(std::move(listener)));
  }
  void emit(const Args&... args){
    std::uint64_t after=0;
    for(;;){
      const auto next=state_->listeners.upper_bound(after);
      if(next==state_->listeners.end())break;
      after=next->first;
      auto listener=next->second;
      (*listener)(args...);
    }
  }
  void clear(){state_->listeners.clear();state_->identities.clear();}
};
}
