#include "cwd/SetCwd.hpp"
#include "runtime/Encoding.hpp"
#include "runtime/IndigenousPath.hpp"
#include "shell/PosixPath.hpp"
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <system_error>
#ifndef _WIN32
#include <unistd.h>
#endif

namespace batbox::cwd {
std::string resolvePosix(const std::string& base,const std::string& path){
  if(!path.empty()&&path[0]=='/')return shell::joinPosixPath(path,"");
  const auto absoluteBase=base.empty()?std::filesystem::current_path().string():
    (base[0]=='/'?base:shell::joinPosixPath(std::filesystem::current_path().string(),base));
  // Node path.resolve returns a path without a trailing slash.
  auto result=shell::joinPosixPath(absoluteBase,path);
  while(result.size()>1&&result.back()=='/')result.pop_back();
  return result;
}
void setCwd(State& state,const std::string& path,
  const std::optional<std::string>& relativeTo,const SetCwdServices& services){
  if(!services.fsCwd||!services.realpathSync)
    throw std::invalid_argument("setCwd filesystem imports required");
#ifdef _WIN32
  const auto indigenous=runtime::pathFromUtf8(path);
  const auto base=runtime::pathFromUtf8(
    relativeTo&& !relativeTo->empty()?*relativeTo:services.fsCwd());
  const auto resolved=runtime::pathToUtf8((indigenous.is_absolute()?indigenous:
    base/indigenous).lexically_normal());
#else
  const auto resolved=!path.empty()&&path[0]=='/'?path:
    resolvePosix(relativeTo&& !relativeTo->empty()?*relativeTo:services.fsCwd(),path);
#endif
  std::string physical;
  try{physical=services.realpathSync(resolved);}
  catch(const std::system_error& error){
    if(error.code()==std::errc::no_such_file_or_directory)
      throw std::runtime_error("Path \""+resolved+"\" does not exist");
    throw;
  }
  setCwdState(state,physical);
}
SetCwdServices indigenousSetCwdServices(){
  return {
    []{return runtime::pathToUtf8(std::filesystem::current_path());},
    [](const std::string& path){
#ifdef _WIN32
      return runtime::pathToUtf8(std::filesystem::canonical(runtime::pathFromUtf8(path)));
#else
      const auto encoded=value::encodeUtf8(path);
      char* raw=::realpath(encoded.c_str(),nullptr);
      if(!raw)throw std::system_error(errno,std::generic_category(),"realpath");
      std::string result(raw);std::free(raw);return result;
#endif
    }
  };
}
} // namespace batbox::cwd
