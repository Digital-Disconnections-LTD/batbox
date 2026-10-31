#include "shell/ManagedShell.hpp"
#include "shell/WindowsPaths.hpp"
#include "runtime/IndigenousPath.hpp"
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <vector>

namespace batbox::shell {
namespace {
#ifndef _WIN32
bool supported(const std::string& shell){return shell.find("bash")!=std::string::npos||shell.find("zsh")!=std::string::npos;}
bool executable(const std::string& path){
#ifdef _WIN32
  std::error_code error;return !path.empty()&&
    std::filesystem::is_regular_file(runtime::pathFromUtf8(path),error);
#else
  return !path.empty()&&::access(path.c_str(),X_OK)==0;
#endif
}
std::string fromPath(const std::string& executableName,const std::map<std::string,std::string>& env){
  const auto found=env.find("PATH");const std::string path=found==env.end()?"/usr/bin:/bin":found->second;
  std::size_t start=0;
  while(start<=path.size()){
    const auto end=path.find(':',start);const auto dir=path.substr(start,end==std::string::npos?std::string::npos:end-start);
    const auto candidate=(dir.empty()?".":dir)+"/"+executableName;
    if(executable(candidate))return candidate;
    if(end==std::string::npos)break;
    start=end+1;
  }
  return {};
}
#endif
}
std::string findSuitableShell(const std::map<std::string,std::string>& env){
#ifdef _WIN32
  (void)env;return findGitBashPath();
#else
  auto get=[&](const std::string& key){auto it=env.find(key);return it==env.end()?std::string():it->second;};
  const auto override=get("BATBOX_SHELL");
  if(supported(override)&&executable(override))return override;
  const auto preferred=get("SHELL");const bool preferBash=preferred.find("bash")!=std::string::npos;
  std::vector<std::string> candidates;
  if(supported(preferred))candidates.push_back(preferred);
  for(const auto& name:preferBash?std::vector<std::string>{"bash","zsh"}:std::vector<std::string>{"zsh","bash"}){
    auto discovered=fromPath(name,env);if(!discovered.empty())candidates.push_back(discovered);
    for(const auto& dir:{"/bin","/usr/bin","/usr/local/bin","/opt/homebrew/bin"})candidates.push_back(std::string(dir)+"/"+name);
  }
  for(const auto& candidate:candidates)if(executable(candidate))return candidate;
  throw std::runtime_error("No suitable shell found. Batbox requires a POSIX shell environment.");
#endif
}
bool isAutobackgroundingAllowed(const std::string& command,
  const std::function<std::vector<std::string>(const std::string&)>& splitCommand){
  if(!splitCommand)throw std::logic_error("splitCommand dependency not supplied");
  const auto parts=splitCommand(command);
  if(parts.empty())return true;
  const auto base=value::trim(parts[0]);
  if(base.empty())return true;
  return base!="sleep";
}
} // namespace batbox::shell
