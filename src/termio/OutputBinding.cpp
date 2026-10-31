#include "termio/Output.hpp"
#include "fs_operations/FsRoster.hpp"
#include <stdexcept>
namespace batbox::termio {
std::function<void(const std::string&)> bindTerminalWriter(
  std::shared_ptr<fs_operations::WritableStream> stdoutStream){
  if(!stdoutStream)throw std::invalid_argument("Terminal stdout stream required");
  return [stdoutStream=std::move(stdoutStream)](const std::string& text){
    (void)stdoutStream->write(text);
  };
}
}
