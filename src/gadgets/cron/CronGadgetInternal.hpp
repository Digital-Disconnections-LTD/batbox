#pragma once
#include "gadgets/cron/CronGadgets.hpp"
#include <future>
#include <stdexcept>
#include <utility>

namespace batbox::gadgets::cron {
template<class T>std::future<T> ready(T value){
  std::promise<T> promise;promise.set_value(std::move(value));
  return promise.get_future();
}
inline void require(bool bound,const char* name){
  if(!bound)throw std::invalid_argument(
    std::string("Missing Cron tool binding: ")+name);
}
inline std::string field(const Json& value,const char* name){
  return value.is_object()&&value.contains(name)&&
    value.at(name).is_string()?value.at(name).get<std::string>():"";
}
inline ValidationResult invalidValidation(std::string message,int code){
  ValidationResult result;
  result.result=false;result.message=std::move(message);
  result.errorCode=code;return result;
}
}
