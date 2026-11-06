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
} // namespace batbox::interactive::text_input
