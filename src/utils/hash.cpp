#include "utils/hash.hpp"
#include "runtime/JsonValue.hpp"
#include <bit>
namespace batbox::utils {
std::int32_t djb2Hash(std::string_view text){
  std::uint32_t hash=0;
  for(const auto unit:value::utf16CodeUnits(text))hash=hash*31+unit;
  return std::bit_cast<std::int32_t>(hash);
}
}
