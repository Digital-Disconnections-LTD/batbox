#pragma once
#include "diagnostics/SlowOperations.hpp"
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace batbox::diagnostics {
using BeginSlowLogging=std::function<std::function<void()>(
  const std::vector<std::string>&,
  const std::vector<std::optional<Json>>&)>;

template<class Operation>auto withSlowScope(const BeginSlowLogging& begin,
    const std::vector<std::string>& strings,
    const std::vector<std::optional<Json>>& values,
    Operation&& operation)->decltype(operation()){
  if(!begin)throw std::invalid_argument("slowLogging import is required");
  auto dispose=begin(strings,values);
  if(!dispose)throw std::invalid_argument("slowLogging disposer is required");
  using Result=decltype(operation());
  if constexpr(std::is_void_v<Result>){
    try{operation();}
    catch(...){
      const auto original=std::current_exception();
      try{dispose();}
      catch(...){throw SuppressedOperationError(std::current_exception(),original);}
      std::rethrow_exception(original);
    }
    dispose();
  }else{
    Result result;
    try{result=operation();}
    catch(...){
      const auto original=std::current_exception();
      try{dispose();}
      catch(...){throw SuppressedOperationError(std::current_exception(),original);}
      std::rethrow_exception(original);
    }
    dispose();
    return result;
  }
}
}
