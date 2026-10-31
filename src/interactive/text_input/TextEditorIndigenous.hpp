#pragma once

#include "interactive/text_input/TextEditor.hpp"
#include "keyboard_input/Keyboard.hpp"
#include <memory>

namespace batbox::interactive::text_input {

// Supplies the concrete clock and ANSI-strip dependency. Application callbacks
// such as history, notifications and submission stay bound to their owners.
EditorCallbacks indigenousEditorCallbacks(EditorCallbacks callbacks);

// Install this closure into InteractionImports::editPoetryKey. Its shared owner
// keeps double-press timers and kill/yank state alive for the poetry lifetime.
std::function<void(keyboard_input::KeyRef,interactive::InputState&)>
bindPoetryEditor(std::shared_ptr<TextEditor> editor);

} // namespace batbox::interactive::text_input
