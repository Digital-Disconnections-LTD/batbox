#include "keyboard_input/Dispatch.hpp"
#include "termio/Csi.hpp"
#include <algorithm>

namespace batbox::keyboard_input {
namespace {
bool isInteraction(const ParsedInput& item){
  if(std::holds_alternative<KeyRef>(item))return true;
  const auto mouse=std::get_if<MouseRef>(&item);
  if(!mouse)return false;
  const auto button=toUint32((*mouse)->button);
  return !((button&0x20u)!=0&&(button&0x03u)==3);
}
void emitFocus(terminal_events::EventEmitter& emitter,const char* name){
  auto event=std::make_shared<terminal_events::TerminalFocusEvent>(name);
  emitter.emit(name,{terminal_events::EventArg::fromEvent(std::move(event))});
}
}
void processKeysInBatch(DispatchBindings& app,
    const std::vector<ParsedInput>& items){
  if(std::any_of(items.begin(),items.end(),isInteraction))
    app.updateLastInteractionTime();
  for(const auto& item:items){
    if(const auto response=std::get_if<ParsedResponse>(&item)){
      app.querier.onResponse(response->response);
      continue;
    }
    if(const auto mouse=std::get_if<MouseRef>(&item)){
      app.handleMouseEvent(*mouse);
      continue;
    }
    const auto& key=std::get<KeyRef>(item);
    const auto& sequence=key->sequence;
    if(sequence&&*sequence==termio::FOCUS_IN){
      app.handleTerminalFocus(true);
      emitFocus(app.events,"terminalfocus");
      continue;
    }
    if(sequence&&*sequence==termio::FOCUS_OUT){
      app.handleTerminalFocus(false);
      if(app.selectionIsDragging()){
        app.finishSelection();
        app.onSelectionChange();
      }
      emitFocus(app.events,"terminalblur");
      continue;
    }
    if(!app.getTerminalFocused())app.setTerminalFocused(true);
    if(key->name&&*key->name=="z"&&key->ctrl&&app.supportsSuspend){
      app.handleSuspend();
      continue;
    }
    app.handleInput(sequence);
    auto event=std::make_shared<terminal_events::InputEvent>(key);
    app.events.emit("input",{terminal_events::EventArg::fromEvent(event)});
    app.dispatchKeyboardEvent(key);
  }
}
} // namespace batbox::keyboard_input
