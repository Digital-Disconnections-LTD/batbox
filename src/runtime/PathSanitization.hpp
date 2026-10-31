#pragma once
#include <functional>
#include <string>
namespace batbox::runtime {
inline constexpr std::size_t MAX_SANITIZED_LENGTH=200;
std::string sanitizePath(const std::string&,
  const std::function<std::string(const std::string&)>& longPathHash={});
}
