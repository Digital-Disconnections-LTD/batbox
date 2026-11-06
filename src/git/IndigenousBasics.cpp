#include "git/GitBasics.hpp"
#include "runtime/Encoding.hpp"
#include "runtime/IndigenousPath.hpp"
#include <filesystem>
#ifndef _WIN32
#include <unistd.h>
#endif

namespace batbox::git {
std::function<std::optional<std::string>(const std::string&)> indigenousWhichSync(
  const std::string& pathEnvironment){
  return [pathEnvironment](const std::string& executable)->std::optional<std::string>{
    if(executable.empty()||executable.find('/')!=std::string::npos||
       executable.find('\\')!=std::string::npos)return std::nullopt;
    std::size_t start=0;
    while(start<=pathEnvironment.size()){
#ifdef _WIN32
      const auto end=pathEnvironment.find(';',start);
      auto dir=pathEnvironment.substr(start,
        end==std::string::npos?std::string::npos:end-start);
      if(dir.size()>1&&dir.front()=='"'&&dir.back()=='"')dir=dir.substr(1,dir.size()-2);
      if(!dir.empty()){
        const auto directory=runtime::pathFromUtf8(dir);
        auto candidate=directory/runtime::pathFromUtf8(executable);
        if(candidate.extension().empty())candidate+=L".exe";
        std::error_code error;
        if(std::filesystem::is_regular_file(candidate,error))
          return runtime::pathToUtf8(candidate);
      }
#else
      const auto end=pathEnvironment.find(':',start);
      const auto dir=pathEnvironment.substr(start,
        end==std::string::npos?std::string::npos:end-start);
      const auto candidate=(dir.empty()?std::string("."):dir)+"/"+executable;
      const auto encoded=value::encodeUtf8(candidate);
      if(::access(encoded.c_str(),X_OK)==0)return candidate;
#endif
      if(end==std::string::npos)break;
      start=end+1;
    }
    return std::nullopt;
  };
}
} // namespace batbox::git
