#pragma once
#include "runtime/AsyncLaunch.hpp"
#include "event_loop/Deferred.hpp"
#include <functional>
#include <memory>
#include <thread>

namespace batbox::locking {
template<class T>event_loop::Deferred<T> runJob(event_loop::Loop& loop,
    std::function<T()> job){
  event_loop::Deferred<T> output(loop);
  auto hold=std::make_shared<event_loop::Loop::KeepAlive>(loop.hold());
  output.then([hold](const T*,const std::exception_ptr&){hold->release();});
  try{
    ::batbox::runtime::contextThread([output,job=std::move(job),hold]{
      try{output.resolve(job());}
      catch(...){output.reject(std::current_exception());}
    }).detach();
  }catch(...){hold->release();output.reject(std::current_exception());}
  return output;
}
}
