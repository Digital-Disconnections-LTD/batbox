#include "terminal_events/Dispatcher.hpp"
#include <array>

namespace batbox::terminal_events {
namespace {
const std::map<std::string,HandlerProps> handlers={
  {"keydown",{"onKeyDown","onKeyDownCapture"}},
  {"focus",{"onFocus","onFocusCapture"}},
  {"blur",{"onBlur","onBlurCapture"}},
  {"paste",{"onPaste","onPasteCapture"}},
  {"resize",{"onResize",""}},
  {"click",{"onClick",""}},
};
const std::array<std::string,12> handlerProps={
  "onKeyDown","onKeyDownCapture","onFocus","onFocusCapture",
  "onBlur","onBlurCapture","onPaste","onPasteCapture","onResize",
  "onClick","onMouseEnter","onMouseLeave"};
}
const HandlerProps* handlerForEvent(const std::string& type){
  const auto found=handlers.find(type);
  return found==handlers.end()?nullptr:&found->second;
}
bool isEventHandlerProp(const std::string& property){
  for(const auto& name:handlerProps)if(name==property)return true;
  return false;
}
} // namespace batbox::terminal_events
