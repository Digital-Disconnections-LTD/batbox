#include "termio/Capabilities.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <array>
namespace batbox::termio {
namespace {
bool present(const std::optional<std::string>& value){return value&&!value->empty();}
bool contains(const std::optional<std::string>& value,const std::string& text){return value&&value->find(text)!=std::string::npos;}
}
CapabilitiesModule::CapabilitiesModule(CapabilitiesServices services):services_(std::move(services)),
  syncOutputSupported_(isSynchronizedOutputSupported()){}
bool CapabilitiesModule::isProgressReportingAvailable()const{
  if(!services_.stdoutIsTTY())return false;
  const auto& env=services_.environment;
  if(present(env.getenv("WT_SESSION")))return false;
  if(present(env.getenv("ConEmuANSI"))||present(env.getenv("ConEmuPID"))||present(env.getenv("ConEmuTask")))return true;
  const auto version=services_.coerceVersion(env.getenv("TERM_PROGRAM_VERSION"));
  if(!version)return false;
  if(env.getenv("TERM_PROGRAM")==std::optional<std::string>("ghostty"))return services_.versionGte(*version,"1.2.0");
  if(env.getenv("TERM_PROGRAM")==std::optional<std::string>("iTerm.app"))return services_.versionGte(*version,"3.6.6");
  return false;
}
bool CapabilitiesModule::isSynchronizedOutputSupported()const{
  const auto& env=services_.environment;
  if(present(env.getenv("TMUX")))return false;
  const auto termProgram=env.getenv("TERM_PROGRAM");
  const auto term=env.getenv("TERM");
  static const std::array<std::string,7> modern={"iTerm.app","WezTerm","WarpTerminal","ghostty","contour","vscode","alacritty"};
  if(termProgram&&std::find(modern.begin(),modern.end(),*termProgram)!=modern.end())return true;
  if(contains(term,"kitty")||present(env.getenv("KITTY_WINDOW_ID")))return true;
  if(term==std::optional<std::string>("xterm-ghostty"))return true;
  if(term&&term->starts_with("foot"))return true;
  if(contains(term,"alacritty"))return true;
  if(present(env.getenv("ZED_TERM")))return true;
  if(present(env.getenv("WT_SESSION")))return true;
  const auto vteVersion=env.getenv("VTE_VERSION");
  if(present(vteVersion)&&value::parseInt10(*vteVersion)>=6800)return true;
  return false;
}
void CapabilitiesModule::setXtversionName(const std::string& name){if(!xtversionName_)xtversionName_=name;}
bool CapabilitiesModule::isXtermJs()const{
  if(services_.environment.getenv("TERM_PROGRAM")==std::optional<std::string>("vscode"))return true;
  return xtversionName_&&xtversionName_->starts_with("xterm.js");
}
bool CapabilitiesModule::supportsExtendedKeys()const{
  const auto terminal=services_.terminalName().value_or("");
  static const std::array<std::string,6> names={"iTerm.app","kitty","WezTerm","ghostty","tmux","windows-terminal"};
  return std::find(names.begin(),names.end(),terminal)!=names.end();
}
bool CapabilitiesModule::hasCursorUpViewportYankBug()const{
  return services_.environment.platform=="win32"||present(services_.environment.getenv("WT_SESSION"));
}
}
