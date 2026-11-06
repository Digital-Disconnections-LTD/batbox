#pragma once
#include "terminal_events/ParsedKey.hpp"
#include <string>
#include <utility>

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
} // namespace batbox::terminal_events
