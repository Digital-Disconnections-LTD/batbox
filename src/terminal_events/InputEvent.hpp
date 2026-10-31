#pragma once
#include "terminal_events/Event.hpp"
#include "terminal_events/ParsedKey.hpp"
#include <memory>

namespace batbox::terminal_events {
struct Key {
  bool upArrow=false,downArrow=false,leftArrow=false,rightArrow=false;
  bool pageDown=false,pageUp=false,wheelUp=false,wheelDown=false;
  bool home=false,end=false,returnKey=false,escape=false;
  bool ctrl=false,shift=false,fn=false,tab=false,backspace=false;
  bool deleteKey=false,meta=false,super=false;
};
bool isNonAlphanumericKey(const std::string& name);
std::pair<Key,std::string> parseInputKey(const ParsedKey& keypress);
class InputEvent:public Event {
public:
  explicit InputEvent(std::shared_ptr<ParsedKey> keypress);
  const std::shared_ptr<ParsedKey> keypress;
  const Key key;
  const std::string input;
private:
  InputEvent(std::shared_ptr<ParsedKey> keypress,
    std::pair<Key,std::string> parsed);
};
} // namespace batbox::terminal_events
