#include "termio/OscLink.hpp"
#include "runtime/ArrayJoin.hpp"
namespace batbox::termio {
std::string osc(const Json& parts,const std::string& terminal){
  const auto terminator=terminal=="kitty"?"\x1b\\":"\x07";
  return std::string("\x1b]")+value::arrayJoin(parts,";")+terminator;
}
}
