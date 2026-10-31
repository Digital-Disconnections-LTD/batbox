#pragma once
#include "runtime/Platform.hpp"
#include <algorithm>
#include <string>

namespace batbox::shell {
inline std::string randomPlaceholderSalt(){
  auto value=platform::randomUUID();
  value.erase(std::remove(value.begin(),value.end(),'-'),value.end());
  return value.substr(0,16);
}
} // namespace batbox::shell
