#include "termio/Clear.hpp"
#include "termio/Csi.hpp"
#include <cstdlib>
namespace batbox::termio {
namespace {
bool present(const std::optional<std::string>& value){return value&&!value->empty();}
const std::string CURSOR_HOME_WINDOWS=csi(Json::array({0,"f"}));
}
bool isWindowsTerminal(const TerminalEnvironment& env){return env.platform=="win32"&&present(env.getenv("WT_SESSION"));}
bool isMintty(const TerminalEnvironment& env){
  if(env.getenv("TERM_PROGRAM")==std::optional<std::string>("mintty"))return true;
  if(env.platform=="win32"&&present(env.getenv("MSYSTEM")))return true;
  return false;
}
bool isModernWindowsTerminal(const TerminalEnvironment& env){
  if(isWindowsTerminal(env))return true;
  if(env.platform=="win32"&&env.getenv("TERM_PROGRAM")==std::optional<std::string>("vscode")&&present(env.getenv("TERM_PROGRAM_VERSION")))return true;
  if(isMintty(env))return true;
  return false;
}
std::string getClearTerminalSequence(const TerminalEnvironment& env){
  if(env.platform=="win32"){
    if(isModernWindowsTerminal(env))return ERASE_SCREEN+ERASE_SCROLLBACK+CURSOR_HOME;
    return ERASE_SCREEN+CURSOR_HOME_WINDOWS;
  }
  return ERASE_SCREEN+ERASE_SCROLLBACK+CURSOR_HOME;
}
TerminalEnvironment indigenousTerminalEnvironment(){
  TerminalEnvironment env;
#if defined(_WIN32)
  env.platform="win32";
#elif defined(__APPLE__)
  env.platform="darwin";
#else
  env.platform="linux";
#endif
  env.getenv=[](const std::string& key)->std::optional<std::string>{
    const auto value=std::getenv(key.c_str());if(!value)return {};return std::string(value);
  };
  return env;
}
}
