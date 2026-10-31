#include "memory_prefetch/BatMemoryPath.hpp"
#include "runtime/PathSanitization.hpp"
#include "shell/PosixPath.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::memory_prefetch {
namespace {
std::string join(const std::string& a,const std::string& b){
  return shell::joinPosixPath(a,b);
}
std::string withSlash(std::string value){
  if(value.empty()||value.back()!='/')value+='/';return value;
}
}
std::string getBatMemoryDir(const std::string& batType,
  const std::string& scope,const std::string& cwd,
  const instructions::MemoryPathServices& paths){
  std::string directory=batType;
  std::replace(directory.begin(),directory.end(),':','-');
  if(scope=="project")
    return withSlash(join(join(join(cwd,".batbox"),"agent-memory"),directory));
  if(scope=="user")
    return withSlash(join(join(instructions::getMemoryBaseDir(paths),
      "agent-memory"),directory));
  if(scope=="local"){
    const auto& remote=value::property(paths.environment,paths.keys.remoteMemoryDir);
    if(value::truthy(remote)&&remote.is_string()){
      if(!paths.findCanonicalGitRoot)
        throw std::invalid_argument("Canonical Git root import required");
      const auto canonical=paths.findCanonicalGitRoot(paths.projectRoot);
      const auto project=runtime::sanitizePath(
        canonical.value_or(paths.projectRoot),paths.longPathHash);
      return withSlash(join(join(join(join(remote.get<std::string>(),
        "projects"),project),"agent-memory-local"),directory));
    }
    return withSlash(join(join(join(cwd,".batbox"),
      "agent-memory-local"),directory));
  }
  throw std::invalid_argument("Unknown agent memory scope: "+scope);
}
} // namespace batbox::memory_prefetch
