#pragma once
#include "termio/Clear.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <memory>
namespace batbox::fs_operations {class WritableStream;}
namespace batbox::termio {
struct OutputServices {
  std::function<std::string()> getClearTerminalSequence;
  std::function<std::string(const std::string&)> link;
};
void writeDiffToTerminal(const std::function<void(const std::string&)>& write,
  const Json& diff,bool skipSyncMarkers,const OutputServices& services);
OutputServices indigenousOutputServices(TerminalEnvironment env,
  std::function<std::string()> terminalName);
std::function<void(const std::string&)> bindTerminalWriter(
  std::shared_ptr<fs_operations::WritableStream> stdoutStream);
}
