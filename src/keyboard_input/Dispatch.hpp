#pragma once
#include "keyboard_input/Keyboard.hpp"
#include "terminal_events/Emitter.hpp"
#include "terminal_events/InputEvent.hpp"
#include "termio/Querier.hpp"
#include <functional>

namespace batbox::keyboard_input {
// Bindings for the App-owned state and methods used by processKeysInBatch.
// The event emitter and querier are concrete indigenous counterparts. The remaining
// callbacks belong to App/selection/DOM modules, not to the keyboard parser.
struct DispatchBindings {
  termio::TerminalQuerier& querier;
  terminal_events::EventEmitter& events;
  std::function<void()> updateLastInteractionTime;
  std::function<void(MouseRef)> handleMouseEvent;
  std::function<void(bool)> handleTerminalFocus;
  std::function<bool()> selectionIsDragging;
  std::function<void()> finishSelection;
  std::function<void()> onSelectionChange;
  std::function<bool()> getTerminalFocused;
  std::function<void(bool)> setTerminalFocused;
  bool supportsSuspend=true;
  std::function<void()> handleSuspend;
  std::function<void(const std::optional<std::string>&)> handleInput;
  std::function<void(KeyRef)> dispatchKeyboardEvent;
};

void processKeysInBatch(DispatchBindings& app,
  const std::vector<ParsedInput>& items);
} // namespace batbox::keyboard_input
