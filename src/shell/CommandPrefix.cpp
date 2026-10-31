#include "shell/CommandQuoting.hpp"
#include "shell/ShellQuote.hpp"
#include "runtime/JsonValue.hpp"

namespace batbox::shell_command {
std::string formatShellPrefixCommand(const std::string& prefix,const std::string& command){
  const auto code=value::utf16CodeUnits(prefix);
  const auto pos=code.rfind(u" -");
  if(pos!=std::u16string::npos&&pos>0){
    const auto execPath=value::fromUtf16(std::u16string_view(code).substr(0,pos));
    const auto args=value::fromUtf16(std::u16string_view(code).substr(pos+1));
    return shell_quote::quote({Json(execPath)})+" "+args+" "+shell_quote::quote({Json(command)});
  }
  return shell_quote::quote({Json(prefix)})+" "+shell_quote::quote({Json(command)});
}
} // namespace batbox::shell_command
