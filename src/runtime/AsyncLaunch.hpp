#pragma once
#include "event_loop/AsyncContext.hpp"
#include <future>
#include <thread>
namespace batbox::runtime {
template<class F,class... Args>auto contextAsync(std::launch policy,F&& callback,Args&&... args){
  return std::async(policy,bindAsyncContext(std::forward<F>(callback)),std::forward<Args>(args)...);
}
template<class F,class... Args>std::thread contextThread(F&& callback,Args&&... args){
  return std::thread(bindAsyncContext(std::forward<F>(callback)),std::forward<Args>(args)...);
}
}
