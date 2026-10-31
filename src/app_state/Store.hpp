#pragma once
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace batbox::app_state {
template<class T>struct ObjectIs {
  bool operator()(const T& left,const T& right)const{
    if constexpr(std::is_floating_point_v<T>){
      if(std::isnan(left)&&std::isnan(right))return true;
      if(left==T(0)&&right==T(0))
        return std::signbit(left)==std::signbit(right);
      return left==right;
    }else return std::equal_to<T>{}(left,right);
  }
};
template<class T>struct ObjectIs<std::shared_ptr<T>> {
  bool operator()(const std::shared_ptr<T>& left,
    const std::shared_ptr<T>& right)const{return left==right;}
};

template<class T,class Equals=ObjectIs<T>>class Store {
public:
  using Listener=std::function<void()>;
  using ListenerRef=std::shared_ptr<Listener>;
  using Updater=std::function<T(const T&)>;
  struct Change {T newState,oldState;};
  using OnChange=std::function<void(const Change&)>;

  explicit Store(T initialState,OnChange onChange={},Equals equal={})
    :core_(std::make_shared<Core>(std::move(initialState),
      std::move(onChange),std::move(equal))){}

  T getState()const{return core_->state;}
  void setState(const Updater& updater){
    const T previous=core_->state;
    const T next=updater(previous);
    if(core_->equal(next,previous))return;
    core_->state=next;
    if(core_->onChange)core_->onChange(Change{next,previous});
    for(std::size_t index=0;index<core_->listeners.size();++index){
      if(!core_->listeners[index].active)continue;
      const auto listener=core_->listeners[index].value;
      (*listener)();
    }
  }
  std::function<void()> subscribe(ListenerRef listener){
    for(const auto& entry:core_->listeners)
      if(entry.active&&entry.value==listener)return unsubscribe(listener);
    core_->listeners.push_back({std::move(listener),true});
    return unsubscribe(core_->listeners.back().value);
  }
private:
  struct Entry {ListenerRef value;bool active;};
  struct Core {
    Core(T initialState,OnChange change,Equals compare):
      state(std::move(initialState)),onChange(std::move(change)),
      equal(std::move(compare)){}
    T state;
    OnChange onChange;
    Equals equal;
    std::vector<Entry> listeners;
  };
  std::shared_ptr<Core> core_;
  std::function<void()> unsubscribe(ListenerRef listener)const{
    const std::weak_ptr<Core> core=core_;
    const std::weak_ptr<Listener> target=listener;
    return [core,target]{
      if(const auto active=core.lock();active)
        for(auto& entry:active->listeners)
          if(entry.active&&entry.value==target.lock()){
            entry.active=false;entry.value.reset();break;
          }
    };
  }
};
template<class T,class Equals=ObjectIs<T>>Store<T,Equals> createStore(
  T initialState,typename Store<T,Equals>::OnChange onChange={},
  Equals equal={}){
  return Store<T,Equals>(std::move(initialState),std::move(onChange),
    std::move(equal));
}
} // namespace batbox::app_state
