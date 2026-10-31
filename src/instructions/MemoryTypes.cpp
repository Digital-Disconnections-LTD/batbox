#include "instructions/MemoryTypes.hpp"

namespace batbox::instructions {
std::optional<std::string> parseMemoryType(const Json& raw){
  if(!raw.is_string())return std::nullopt;
  const auto type=raw.get<std::string>();
  for(const char* candidate:{"user","feedback","project","reference"})
    if(type==candidate)return type;
  return std::nullopt;
}
} // namespace batbox::instructions
