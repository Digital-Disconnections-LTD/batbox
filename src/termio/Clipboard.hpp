#pragma once
#include "termio/Clear.hpp"
#include "process/ExecFileNoThrow.hpp"
namespace batbox::termio {
struct ClipboardServices {
  TerminalEnvironment environment;
  std::function<std::string()> terminalName;
  std::function<event_loop::Deferred<process::ExecResult>(const std::string&,
    const std::vector<std::string>&,const process::ExecOptions&)> execFileNoThrow;
};
ClipboardServices indigenousClipboardServices(event_loop::Loop& loop,
  TerminalEnvironment environment,std::function<std::string()> terminalName,
  process::ExecImports imports);
class ClipboardModule {
public:
  ClipboardModule(event_loop::Loop& loop,ClipboardServices services);
  event_loop::Deferred<bool> tmuxLoadBuffer(const std::string& text);
  event_loop::Deferred<std::string> setClipboard(const std::string& text);
  void copyIndigenous(const std::string& text);
private:
  struct Core;
  std::shared_ptr<Core> core_;
};
}
