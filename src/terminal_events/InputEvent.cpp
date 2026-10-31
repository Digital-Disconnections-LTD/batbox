#include "terminal_events/InputEvent.hpp"
#include "keyboard_input/Keyboard.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <regex>
#include <stdexcept>

namespace batbox::terminal_events {
bool isNonAlphanumericKey(const std::string& name){
  const auto& names=keyboard_input::nonAlphanumericKeys();
  return std::find(names.begin(),names.end(),name)!=names.end();
}
namespace {
bool nameIs(const ParsedKey& keypress,const char* value){
  return keypress.name&&*keypress.name==value;
}
bool hasName(const ParsedKey& keypress){
  return keypress.name&&!keypress.name->empty();
}
bool startsWith(const std::string& value,const std::string& prefix){
  return value.starts_with(prefix);
}
bool endsWith(const std::string& value,const std::string& suffix){
  return value.ends_with(suffix);
}
bool mouseFragment(const std::string& input){
  static const std::regex pattern(R"(^\[<\d+;\d+;\d+[Mm])");
  return std::regex_search(input,pattern);
}
bool csiU(const std::string& input){
  static const std::regex pattern(R"(^\[\d)");
  return std::regex_search(input,pattern);
}
}
std::pair<Key,std::string> parseInputKey(const ParsedKey& keypress){
  Key key;
  key.upArrow=nameIs(keypress,"up");
  key.downArrow=nameIs(keypress,"down");
  key.leftArrow=nameIs(keypress,"left");
  key.rightArrow=nameIs(keypress,"right");
  key.pageDown=nameIs(keypress,"pagedown");
  key.pageUp=nameIs(keypress,"pageup");
  key.wheelUp=nameIs(keypress,"wheelup");
  key.wheelDown=nameIs(keypress,"wheeldown");
  key.home=nameIs(keypress,"home");
  key.end=nameIs(keypress,"end");
  key.returnKey=nameIs(keypress,"return");
  key.escape=nameIs(keypress,"escape");
  key.fn=keypress.fn;
  key.ctrl=keypress.ctrl;
  key.shift=keypress.shift;
  key.tab=nameIs(keypress,"tab");
  key.backspace=nameIs(keypress,"backspace");
  key.deleteKey=nameIs(keypress,"delete");
  key.meta=keypress.meta||nameIs(keypress,"escape")||keypress.option;
  key.super=keypress.super;

  std::string input=keypress.ctrl?keypress.name.value_or(""):
    keypress.sequence.value_or("");
  if(keypress.ctrl&&input=="space")input=" ";
  if(keypress.code&&!keypress.code->empty()&&!hasName(keypress))input="";
  if(!hasName(keypress)&&mouseFragment(input))input="";
  if(startsWith(input,"\x1b"))input=value::slice(input,1);
  bool processedAsSpecialSequence=false;
  if(csiU(input)&&endsWith(input,"u")){
    if(!hasName(keypress))input="";
    else input=nameIs(keypress,"space")?" ":
      nameIs(keypress,"escape")?"":*keypress.name;
    processedAsSpecialSequence=true;
  }
  if(startsWith(input,"[27;")&&endsWith(input,"~")){
    if(!hasName(keypress))input="";
    else input=nameIs(keypress,"space")?" ":
      nameIs(keypress,"escape")?"":*keypress.name;
    processedAsSpecialSequence=true;
  }
  if(startsWith(input,"O")&&value::stringLength(input)==2&&hasName(keypress)&&
      value::stringLength(*keypress.name)==1){
    input=*keypress.name;
    processedAsSpecialSequence=true;
  }
  if(!processedAsSpecialSequence&&hasName(keypress)&&
      isNonAlphanumericKey(*keypress.name))input="";
  if(value::stringLength(input)==1){
    const auto units=value::utf16CodeUnits(input);
    if(!units.empty()&&units[0]>=u'A'&&units[0]<=u'Z')key.shift=true;
  }
  return {key,input};
}
InputEvent::InputEvent(std::shared_ptr<ParsedKey> keypress):
  InputEvent(keypress,[&]{
    if(!keypress)throw std::invalid_argument("Parsed key required");
    return parseInputKey(*keypress);}()){}
InputEvent::InputEvent(std::shared_ptr<ParsedKey> source,
    std::pair<Key,std::string> parsed):
  keypress(std::move(source)),key(std::move(parsed.first)),
  input(std::move(parsed.second)){}
} // namespace batbox::terminal_events
