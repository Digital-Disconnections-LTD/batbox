#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/AsyncLaunch.hpp"
#include <future>
#include <type_traits>

namespace batbox::application {
// Synchronous services run on a worker where necessary;
// live application state and Promise continuations stay on the owner loop.
template<class T>T onOwnerAndWait(event_loop::Loop::Dispatcher owner,
    std::function<T()> callback){
  if(!owner.valid())throw std::runtime_error("Application event loop has closed");
  if(owner.isOwnerThread())return callback();
  auto result=std::make_shared<std::promise<T>>();auto future=result->get_future();
  owner.postMission([result,callback=std::move(callback)]{
    try{
      if constexpr(std::is_void_v<T>){callback();result->set_value();}
      else result->set_value(callback());
    }catch(...){result->set_exception(std::current_exception());}
  });
  return future.get();
}
template<class T>T awaitOnOwner(event_loop::Loop::Dispatcher owner,
    std::function<event_loop::Deferred<T>()> callback){
  if(!owner.valid())throw std::runtime_error("Application event loop has closed");
  if(owner.isOwnerThread())throw std::logic_error("Blocking Promise wait on application owner");
  auto result=std::make_shared<std::promise<T>>();auto future=result->get_future();
  owner.postMission([owner,result,callback=std::move(callback)]{
    try{
      auto pending=callback();
      if(!owner.sameLoop(pending.dispatcher()))
        throw std::logic_error("Application provider returned a foreign Promise");
      pending.then([result](const T* value,const std::exception_ptr& error){
        if(error)result->set_exception(error);
        else result->set_value(*value);
      });
    }catch(...){result->set_exception(std::current_exception());}
  });
  return future.get();
}
template<class T>event_loop::Deferred<T> runInWorker(event_loop::Loop& owner,
    std::function<T()> callback){
  if(!owner.isOwnerThread())throw std::logic_error("Worker launch requires application owner");
  event_loop::Deferred<T> result(owner);
  auto keepAlive=std::make_shared<event_loop::Loop::KeepAlive>(owner.hold());
  const auto dispatcher=owner.dispatcher();
  try{
    runtime::contextThread([result,dispatcher,keepAlive,callback=std::move(callback)]{
      try{result.resolve(callback());}catch(...){result.reject(std::current_exception());}
      dispatcher.postMission([keepAlive]{keepAlive->release();});
    }).detach();
  }catch(...){keepAlive->release();result.reject(std::current_exception());}
  return result;
}
} // namespace batbox::application
