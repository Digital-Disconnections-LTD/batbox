#include "connectors/Executable.hpp"
#include "runtime/IndigenousPath.hpp"
#include <cstdlib>
#include <system_error>
#include <vector>
#ifdef _WIN32
#include "runtime/IndigenousUnicode.hpp"
#else
#include <unistd.h>
#endif

namespace batbox::connectors {
namespace {
std::string environment(const char* name){
#ifdef _WIN32
  return runtime::environmentUtf8(name);
#else
  const char* value=std::getenv(name);return value?value:"";
#endif
}
bool runnable(const std::filesystem::path& candidate){
  std::error_code error;
  if(!std::filesystem::is_regular_file(candidate,error))return false;
#ifdef _WIN32
  return true;
#else
  return ::access(candidate.c_str(),X_OK)==0;
#endif
}
std::vector<std::string> split(const std::string& text,char separator){
  std::vector<std::string> parts;std::size_t start=0;
  while(start<=text.size()){
    const auto end=text.find(separator,start);
    parts.push_back(text.substr(start,end==std::string::npos?std::string::npos:end-start));
    if(end==std::string::npos)break;
    start=end+1;
  }
  return parts;
}
std::optional<std::filesystem::path> withExtensions(const std::filesystem::path& base){
  if(runnable(base))return base;
#ifdef _WIN32
  auto extensions=environment("PATHEXT");
  if(extensions.empty())extensions=".COM;.EXE;.BAT;.CMD";
  for(const auto& extension:split(extensions,';')){
    if(extension.empty())continue;
    auto candidate=base;candidate+=runtime::pathFromUtf8(extension);
    if(runnable(candidate))return candidate;
  }
#endif
  return std::nullopt;
}
}

std::optional<std::filesystem::path> findExecutable(const std::string& command){
  if(command.empty())return std::nullopt;
  const auto path=runtime::pathFromUtf8(command);
  const bool hasDirectory=command.find('/')!=std::string::npos
#ifdef _WIN32
    ||command.find('\\')!=std::string::npos
#endif
    ;
  if(hasDirectory||path.is_absolute())return withExtensions(path);
#ifdef _WIN32
  constexpr char separator=';';
#else
  constexpr char separator=':';
#endif
  for(const auto& directory:split(environment("PATH"),separator)){
    // An empty PATH entry means the current directory.
    const auto root=directory.empty()?std::filesystem::path("."):runtime::pathFromUtf8(directory);
    if(auto found=withExtensions(root/path))return found;
  }
  return std::nullopt;
}
} // namespace batbox::connectors
