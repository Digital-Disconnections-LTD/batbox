#include "shell/ManagedShell.hpp"
#include <stdexcept>
#include <vector>

namespace batbox::shell {
bool isAutobackgroundingAllowed(const std::string& command,
  const std::function<std::vector<std::string>(const std::string&)>& splitCommand){
  if(!splitCommand)throw std::logic_error("splitCommand dependency not supplied");
  const auto parts=splitCommand(command);
  if(parts.empty())return true;
  const auto segment=value::trimView(parts[0]);
  if(segment.empty())return true;
  // The splitter returns whole segments ("sleep 30"); compare the program.
  return segment.substr(0,segment.find_first_of(" \t\r\n"))!="sleep";
}
} // namespace batbox::shell
