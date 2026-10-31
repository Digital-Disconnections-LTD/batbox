#pragma once
#include "runtime/JsonValue.hpp"
#include <cstdint>
#include <string>
#include <vector>
namespace batbox::text_width {
inline std::vector<std::uint32_t> codePoints(const std::string& text){
  const auto units=value::utf16CodeUnits(text);std::vector<std::uint32_t> points;
  for(std::size_t i=0;i<units.size();++i){std::uint32_t cp=units[i];
    if(cp>=0xd800&&cp<=0xdbff&&i+1<units.size()&&units[i+1]>=0xdc00&&units[i+1]<=0xdfff)
      cp=0x10000+((cp-0xd800)<<10)+(units[++i]-0xdc00);
    points.push_back(cp);
  }
  return points;
}
}
