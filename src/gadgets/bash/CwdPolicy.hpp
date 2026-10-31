#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <string>

namespace batbox::bash_gadget {
struct CwdPolicyServices {
  std::function<std::string()> getCwd;
  std::function<std::string()> getOriginalCwd;
  std::function<bool()> shouldMaintainProjectWorkingDir;
  std::function<bool(const std::string&,const Json&)> pathInAllowedWorkingPath;
  std::function<void(const std::string&)> setCwd;
};
bool resetCwdIfOutsideProject(const Json& gadgetPermissionContext,
                              const CwdPolicyServices& services);
std::string stdErrAppendShellResetMessage(const std::string& stderrText,
                                          const std::string& originalCwd);
}
