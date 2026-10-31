#include "interactive/text_input/TextEditorIndigenous.hpp"
#include "npm_wrap_ansi/Internal.hpp"
#include "runtime/JsonValue.hpp"
#include <chrono>
#include <stdexcept>

namespace batbox::interactive::text_input {
EditorCallbacks indigenousEditorCallbacks(EditorCallbacks callbacks){
  if(!callbacks.nowMs)callbacks.nowMs=[](){
    const auto now=std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration<double,std::milli>(now).count();
  };
  if(!callbacks.stripAnsi)callbacks.stripAnsi=[](const std::string& input){
    return value::fromUtf16(npm_wrap_ansi::stripAnsi(
      value::utf16CodeUnits(input)));
  };
  return callbacks;
}
std::function<void(keyboard_input::KeyRef,interactive::InputState&)>
bindPoetryEditor(std::shared_ptr<TextEditor> editor){
  if(!editor)throw std::invalid_argument("poetry editor required");
  return [editor=std::move(editor)](keyboard_input::KeyRef key,
                                   interactive::InputState& state){
    if(!key)throw std::invalid_argument("Parsed poetry key required");
    editor->onParsedKey(*key,state);
  };
}
} // namespace batbox::interactive::text_input
