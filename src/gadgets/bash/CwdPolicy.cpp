#include "gadgets/bash/CwdPolicy.hpp"
#include <stdexcept>

namespace batbox::bash_gadget {
bool resetCwdIfOutsideProject(const Json& gadgetPermissionContext,
                              const CwdPolicyServices& services){
  if(!services.getCwd||!services.getOriginalCwd||
     !services.shouldMaintainProjectWorkingDir||!services.pathInAllowedWorkingPath||
     !services.setCwd)throw std::invalid_argument("Bash cwd policy imports required");
  const auto cwd=services.getCwd();
  const auto originalCwd=services.getOriginalCwd();
  const bool shouldMaintain=services.shouldMaintainProjectWorkingDir();
  if(shouldMaintain||(cwd!=originalCwd&&
      !services.pathInAllowedWorkingPath(cwd,gadgetPermissionContext))){
    services.setCwd(originalCwd);
    if(!shouldMaintain)return true;
  }
  return false;
}
std::string stdErrAppendShellResetMessage(const std::string& stderrText,
                                          const std::string& originalCwd){
  return value::trim(stderrText)+"\nShell cwd was reset to "+originalCwd;
}
} // namespace batbox::bash_gadget
