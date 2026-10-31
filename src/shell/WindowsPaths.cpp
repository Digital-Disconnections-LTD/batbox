#include "shell/WindowsPaths.hpp"
#include "runtime/IndigenousPath.hpp"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <stdexcept>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "runtime/IndigenousUnicode.hpp"
#endif

namespace batbox::shell {
std::string windowsPathToPosixPath(const std::string& path){
  std::string result=path;
  std::replace(result.begin(),result.end(),'\\','/');
  if(path.size()>=2&&path[0]=='\\'&&path[1]=='\\')return result;
  if(path.size()>=3&&std::isalpha(static_cast<unsigned char>(path[0]))&&
     path[1]==':'&&(path[2]=='\\'||path[2]=='/')){
    result="/";result+=static_cast<char>(std::tolower(static_cast<unsigned char>(path[0])));
    result+=path.substr(2);
    std::replace(result.begin(),result.end(),'\\','/');
  }
  return result;
}
std::string posixPathToWindowsPath(const std::string& path){
  std::string result=path;
  if(path.rfind("//",0)==0){std::replace(result.begin(),result.end(),'/','\\');return result;}
  std::size_t driveAt=std::string::npos,restAt=0;
  if(path.rfind("/cygdrive/",0)==0&&path.size()>=11&&
     std::isalpha(static_cast<unsigned char>(path[10]))&&
     (path.size()==11||path[11]=='/')){driveAt=10;restAt=11;}
  else if(path.size()>=2&&path[0]=='/'&&
    std::isalpha(static_cast<unsigned char>(path[1]))&&
    (path.size()==2||path[2]=='/')){driveAt=1;restAt=2;}
  if(driveAt!=std::string::npos){
    result=static_cast<char>(std::toupper(static_cast<unsigned char>(path[driveAt])));
    result+=':';result+=path.substr(restAt).empty()?"\\":path.substr(restAt);
  }
  std::replace(result.begin(),result.end(),'/','\\');return result;
}
#ifdef _WIN32
namespace {
std::string env(const char* key){return runtime::environmentUtf8(key);}
bool regular(const std::filesystem::path& path){std::error_code error;
  return std::filesystem::is_regular_file(path,error);}
bool samePathText(const std::wstring& left,const std::wstring& right){
  return left.size()==right.size()&&::CompareStringOrdinal(
    left.data(),static_cast<int>(left.size()),right.data(),
    static_cast<int>(right.size()),TRUE)==CSTR_EQUAL;
}
bool underCurrentDirectory(const std::filesystem::path& candidate,
    const std::filesystem::path& cwd){
  const auto directory=candidate.parent_path().native();
  const auto current=cwd.native();
  if(samePathText(directory,current))return true;
  const auto prefix=current+std::filesystem::path::preferred_separator;
  const auto normalized=candidate.native();
  return normalized.size()>=prefix.size()&&
    samePathText(normalized.substr(0,prefix.size()),prefix);
}
std::optional<std::filesystem::path> searchOnPath(const std::string& executable,
    bool excludeCurrentDirectory){
  const auto cwd=std::filesystem::current_path().lexically_normal().make_preferred();
  const auto pathValue=env("PATH");std::size_t start=0;
  while(start<=pathValue.size()){
    auto end=pathValue.find(';',start);
    auto part=pathValue.substr(start,end==std::string::npos?std::string::npos:end-start);
    if(part.size()>1&&part.front()=='"'&&part.back()=='"')part=part.substr(1,part.size()-2);
    if(!part.empty()){
      std::error_code error;
      auto candidate=std::filesystem::absolute(runtime::pathFromUtf8(part)/
        runtime::pathFromUtf8(executable),error).lexically_normal().make_preferred();
      if(!error&&(!excludeCurrentDirectory||!underCurrentDirectory(candidate,cwd))&&
          regular(candidate))
        return candidate;
    }
    if(end==std::string::npos)break;
    start=end+1;
  }
  return std::nullopt;
}
std::optional<std::filesystem::path> searchGit(){
  for(const auto& entry:{"C:\\Program Files\\Git\\cmd\\git.exe",
                         "C:\\Program Files (x86)\\Git\\cmd\\git.exe"}){
    auto path=runtime::pathFromUtf8(entry);if(regular(path))return path;
  }
  return searchOnPath("git.exe",true);
}
}
std::string findGitBashPath(){
  const auto override=env("BATBOX_GIT_BASH_PATH");
  if(!override.empty()){
    if(regular(runtime::pathFromUtf8(override)))return override;
    throw std::runtime_error("BATBOX_GIT_BASH_PATH does not point to bash.exe: "+override);
  }
  if(const auto git=searchGit()){
    auto bash=(git->parent_path()/".."/"bin"/"bash.exe").lexically_normal();
    if(regular(bash))return runtime::pathToUtf8(bash);
  }
  throw std::runtime_error("Batbox on Windows requires Git Bash; install Git for Windows or set BATBOX_GIT_BASH_PATH to bash.exe");
}
std::optional<std::string> findPowerShellPath(){
  for(const auto* name:{"pwsh.exe","powershell.exe"})
    if(const auto found=searchOnPath(name,false))return runtime::pathToUtf8(*found);
  return std::nullopt;
}
#else
std::string findGitBashPath(){throw std::runtime_error("Git Bash discovery requires Windows");}
std::optional<std::string> findPowerShellPath(){return std::nullopt;}
#endif
}
