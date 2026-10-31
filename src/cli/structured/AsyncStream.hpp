#pragma once
#include "event_loop/Deferred.hpp"
#include <deque>
#include <exception>
#include <functional>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace batbox::cli::structured {
template<class T>struct StreamItem {
  bool done=false;
  std::optional<T> value;
};

template<class T>class AsyncStream {
public:
  explicit AsyncStream(event_loop::Loop& loop,std::function<void()> returned={}):
    loop_(loop),returned_(std::move(returned)){}
  AsyncStream& iterate(){
    if(started_)throw std::runtime_error("Stream can only be iterated once");
    started_=true;return *this;
  }
  event_loop::Deferred<StreamItem<T>> next(){
    event_loop::Deferred<StreamItem<T>> result(loop_);
    if(!queue_.empty()){
      auto value=std::move(queue_.front());queue_.pop_front();
      result.resolve({false,std::move(value)});
    }else if(isDone_)result.resolve({true,{}});
    else if(error_)result.reject(error_);
    else pending_=result;
    return result;
  }
  void enqueue(T value){
    if(pending_){
      auto current=std::move(*pending_);pending_.reset();
      current.resolve({false,std::move(value)});
    }else queue_.push_back(std::move(value));
  }
  void done(){
    isDone_=true;
    if(pending_){
      auto current=std::move(*pending_);pending_.reset();
      current.resolve({true,{}});
    }
  }
  void error(std::exception_ptr failure){
    error_=failure;
    if(pending_){
      auto current=std::move(*pending_);pending_.reset();
      current.reject(failure);
    }
  }
  event_loop::Deferred<StreamItem<T>> returnStream(){
    isDone_=true;
    if(returned_)returned_();
    event_loop::Deferred<StreamItem<T>> result(loop_);
    result.resolve({true,{}});
    return result;
  }
  std::size_t queued()const{return queue_.size();}
  std::vector<T> snapshotQueued()const{
    return {queue_.begin(),queue_.end()};
  }
private:
  event_loop::Loop& loop_;
  std::function<void()> returned_;
  std::deque<T> queue_;
  std::optional<event_loop::Deferred<StreamItem<T>>> pending_;
  std::exception_ptr error_;
  bool isDone_=false,started_=false;
};
}
