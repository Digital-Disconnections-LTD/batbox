#pragma once
#include "terminal_events/Event.hpp"
#include "terminal_events/ParsedKey.hpp"

namespace batbox::terminal_events {
std::string keyFromParsed(const ParsedKey& parsed);
class KeyboardEvent:public TerminalEvent {
public:
  KeyboardEvent(const ParsedKey& parsed,MonotonicNow now);
  const std::string key;
  const bool ctrl,shift,meta,superKey,fn;
};
} // namespace batbox::terminal_events
