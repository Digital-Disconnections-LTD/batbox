#pragma once
#include "diagnostics/ScopeAdapter.hpp"
#include <memory>
#include <stdexcept>

namespace batbox::diagnostics {
inline BeginSlowLogging bindSlowLogging(std::shared_ptr<SlowOperations> operations){
  if(!operations)throw std::invalid_argument("Slow operation owner is required");
  return [operations](const std::vector<std::string>& strings,
      const std::vector<std::optional<Json>>& values){
    std::vector<SlowArgument> arguments;
    arguments.reserve(values.size());
    for(const auto& value:values){
      if(value)arguments.emplace_back(*value);
      else arguments.emplace_back(std::monostate{});
    }
    auto dispose=operations->beginSlowLogging(strings,std::move(arguments));
    return std::function<void()>([operations,dispose=std::move(dispose)]{
      dispose();
    });
  };
}
}
