#include "shell/ShellBootstrap.hpp"
#include <stdexcept>
#include <vector>

namespace batbox::shell {
namespace {
bool supported(const std::string& path){
  return path.find("bash")!=std::string::npos||path.find("zsh")!=std::string::npos;
}
}
std::string findSuitableShell(const DiscoveryServices& services){
  if(!services.isExecutable||!services.which||!services.debug)
    throw std::invalid_argument("shell discovery imports required");
  if(services.shellOverride&&!services.shellOverride->empty()){
    const auto& override=*services.shellOverride;
    if(supported(override)&&services.isExecutable(override)){
      services.debug("Using shell override: "+override);
      return override;
    }
    services.debug("BATBOX_SHELL=\""+override+
      "\" is not a valid bash/zsh path, falling back to detection");
  }
  const auto preferred=services.preferredShell.value_or("");
  const bool preferBash=preferred.find("bash")!=std::string::npos;
  const auto names=preferBash?std::vector<std::string>{"bash","zsh"}:
    std::vector<std::string>{"zsh","bash"};
  std::vector<std::string> candidates;
  if(supported(preferred))candidates.push_back(preferred);
  if(const auto found=services.which(names[0]))candidates.push_back(*found);
  for(const auto& name:names)
    for(const auto* location:{"/bin","/usr/bin","/usr/local/bin","/opt/homebrew/bin"})
      candidates.push_back(std::string(location)+"/"+name);
  if(const auto found=services.which(names[1]))candidates.push_back(*found);
  for(const auto& candidate:candidates)
    if(!candidate.empty()&&services.isExecutable(candidate))return candidate;
  if(services.isExecutable("/bin/sh")){
    services.debug("No bash or zsh found; Bash commands use /bin/sh");
    return "/bin/sh";
  }
  throw std::runtime_error("No shell found for Bash commands: install bash or zsh, or set "
    "BATBOX_SHELL to the path of one");
}
} // namespace batbox::shell
