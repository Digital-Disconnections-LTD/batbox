#pragma once
#include "runtime/IndigenousPath.hpp"

namespace batbox::flights {
inline std::filesystem::path indigenousPath(const std::string& utf8){
  return runtime::pathFromUtf8(utf8);
}
} // namespace batbox::flights
