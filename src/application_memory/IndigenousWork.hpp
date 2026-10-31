#pragma once
#include "event_loop/Deferred.hpp"
#include <optional>

namespace batbox::application_memory {
template<class T,class F>event_loop::Deferred<T> memoryIO(
    event_loop::Loop& loop,F operation){
  event_loop::Deferred<T> output(loop);
  auto value=std::make_shared<std::optional<T>>();
  loop.queueWork([value,operation=std::move(operation)]()mutable{
    value->emplace(operation());
  },[value,output](std::exception_ptr error){
    if(error)output.reject(error);else output.resolve(std::move(**value));
  });
  return output;
}
} // namespace batbox::application_memory
