#pragma once
#include <optional>
#include <string>

namespace batbox::terminal_events {
// Parsed keyboard event data. The parser owner
// supplies these fields; no terminal bytes are interpreted here.
struct ParsedKey {
  bool fn=false,ctrl=false,meta=false,shift=false,option=false,super=false;
  bool isPasted=false;
  std::optional<std::string> name,sequence,raw,code;
};
} // namespace batbox::terminal_events
