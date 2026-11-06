#include "shell/ShellCwdCompletion.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::shell {
void completeShellCwd(const ExecResult* result,bool preventCwdChanges,
  const std::string& cwdFilePath,const std::string& oldCwd,
  const CwdCompletionServices& services){
  if(!services.unlinkFile)throw std::invalid_argument("unlinkFile required");
  if(services.windows&&!services.posixPathToIndigenous)
    throw std::invalid_argument("posixPathToNative required on Windows");
  const auto indigenousPath=services.windows?services.posixPathToIndigenous(cwdFilePath):cwdFilePath;
  if(result&&!preventCwdChanges&&!result->backgroundMissionId){
    if(!services.readFileUtf8||!services.normalizeNFC||!services.setCwd||
       !services.invalidateFlightEnvStash||!services.onCwdChangedForHooks)
      throw std::invalid_argument("cwd completion imports required");
    try{
      auto newCwd=value::trim(services.readFileUtf8(indigenousPath));
      if(services.windows)newCwd=services.posixPathToIndigenous(newCwd);
      if(services.normalizeNFC(newCwd)!=oldCwd){
        services.setCwd(newCwd,oldCwd);
        services.invalidateFlightEnvStash();
        services.onCwdChangedForHooks(oldCwd,newCwd);
      }
    }catch(...){/* A missing or unreadable cwd file leaves the cwd unchanged. */}
  }
  try{services.unlinkFile(indigenousPath);}catch(...){/* Missing after failed pwd. */}
}
} // namespace batbox::shell
