#include "terminal_events/KeyboardEvent.hpp"
#include "runtime/JsonValue.hpp"

namespace batbox::terminal_events {
std::string keyFromParsed(const ParsedKey& parsed){
  const std::string sequence=parsed.sequence.value_or("");
  const std::string name=parsed.name.value_or("");
  if(parsed.ctrl)return name;
  if(value::stringLength(sequence)==1){
    const auto units=value::utf16CodeUnits(sequence);
    const auto code=units[0];
    if(code>=0x20&&code!=0x7f)return sequence;
  }
  if(!name.empty())return name;
  return sequence;
}
KeyboardEvent::KeyboardEvent(const ParsedKey& parsed,MonotonicNow now):
  TerminalEvent("keydown",std::move(now),TerminalEventInit{true,true}),
  key(keyFromParsed(parsed)),ctrl(parsed.ctrl),shift(parsed.shift),
  meta(parsed.meta||parsed.option),superKey(parsed.super),fn(parsed.fn){}
} // namespace batbox::terminal_events
