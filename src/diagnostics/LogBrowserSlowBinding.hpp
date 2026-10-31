#pragma once
#include "diagnostics/SlowOperations.hpp"
#include <memory>
#include <stdexcept>

namespace batbox::diagnostics {
inline std::function<Json(const std::string&)> bindSlowJsonParser(
    std::shared_ptr<SlowOperations> operations){
  if(!operations)throw std::invalid_argument("Slow operations are required");
  return [operations](const std::string& text){return operations->jsonParse(text);};
}
}
