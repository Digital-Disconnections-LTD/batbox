#include "shell/ShellBootstrap.hpp"
#include "runtime/IndigenousPath.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <optional>

namespace batbox::shell {
namespace {
bool executable(const std::filesystem::path& path){
  std::error_code error;return std::filesystem::is_regular_file(path,error);
}
std::optional<std::string> findOnPath(const std::string& name,const std::string& pathEnvironment){
  if(name.empty()||name.find('/')!=std::string::npos||name.find('\\')!=std::string::npos||
      name.find(':')!=std::string::npos)return std::nullopt;
  std::size_t start=0;
  while(start<=pathEnvironment.size()){
    const auto end=pathEnvironment.find(';',start);
    auto part=pathEnvironment.substr(start,end==std::string::npos?std::string::npos:end-start);
    if(part.size()>1&&part.front()=='"'&&part.back()=='"')part=part.substr(1,part.size()-2);
    if(!part.empty()){
      const auto dir=runtime::pathFromUtf8(part);
      const auto candidate=dir/runtime::pathFromUtf8(name);
      if(executable(candidate))return runtime::pathToUtf8(candidate);
      auto extension=runtime::pathToUtf8(candidate.extension());
      std::transform(extension.begin(),extension.end(),extension.begin(),
        [](unsigned char c){return static_cast<char>(std::tolower(c));});
      if(extension!=".exe"&&extension!=".cmd"&&extension!=".bat"){
        const auto withExtension=std::filesystem::path(candidate.wstring()+L".exe");
        if(executable(withExtension))return runtime::pathToUtf8(withExtension);
      }
    }
    if(end==std::string::npos)break;
    start=end+1;
  }
  return std::nullopt;
}
}
bool indigenousIsExecutable(const std::string& shellPath){
  if(shellPath.empty())return false;
  return executable(runtime::pathFromUtf8(shellPath));
}
std::function<event_loop::Deferred<std::optional<std::string>>(const std::string&)>
indigenousWhich(event_loop::Loop& loop,const std::string& pathEnvironment){
  return [&loop,pathEnvironment](const std::string& executableName){
    event_loop::Deferred<std::optional<std::string>> result(loop);
    result.resolve(findOnPath(executableName,pathEnvironment));return result;
  };
}
} // namespace batbox::shell
