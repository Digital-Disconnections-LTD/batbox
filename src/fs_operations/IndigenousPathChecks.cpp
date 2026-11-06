#include "fs_operations/IndigenousPathChecks.hpp"
#include "runtime/IndigenousPath.hpp"
#include "runtime/Unicode.hpp"
#include <filesystem>
#include <stdexcept>
#include <string>

namespace batbox::fs_operations {
namespace {
#ifdef _WIN32
// Drive letters and backslashes: let the Windows path rules decide.
std::string dirname(const std::string& value){
  const auto parent=runtime::pathFromUtf8(value).parent_path();
  return parent.empty()?std::string("."):runtime::pathToUtf8(parent);
}
std::string basename(const std::string& value){
  auto path=runtime::pathFromUtf8(value);
  if(!path.has_filename())path=path.parent_path();
  return runtime::pathToUtf8(path.filename());
}
#else
std::string dirname(const std::string& value){
  if(value.empty())return ".";
  auto end=value.find_last_not_of('/');
  if(end==std::string::npos)return "/";
  const auto slash=value.rfind('/',end);
  if(slash==std::string::npos)return ".";
  if(slash==0)return "/";
  if(slash==1&&value[0]=='/')return "//";
  return value.substr(0,slash);
}
std::string basename(const std::string& value){
  const auto end=value.find_last_not_of('/');
  if(end==std::string::npos)return "";
  const auto slash=value.rfind('/',end);
  return value.substr(slash==std::string::npos?0:slash+1,
    end-(slash==std::string::npos?0:slash+1)+1);
}
#endif
std::string normalize(const std::filesystem::path& value){
  auto result=runtime::pathToUtf8(value.lexically_normal());
  if(result.empty())result=".";
  return result;
}
}
FileView indigenousFileView(){return filesystem::indigenousFileView();}
PathImports indigenousPathImports(std::function<std::string()> cwd,
    std::function<std::string()> home){
  if(!cwd||!home)throw std::invalid_argument("Native cwd and home imports are required");
  PathImports path;
  path.dirname=dirname;path.basename=basename;
  path.join=[](const std::vector<std::string>& parts){
    std::filesystem::path joined;
    for(const auto& part:parts)if(!part.empty())joined/=runtime::pathFromUtf8(part);
    return normalize(joined);
  };
  path.isAbsolute=[](const std::string& value){
    return !value.empty()&&runtime::pathFromUtf8(value).is_absolute();};
  path.resolve=[cwd=std::move(cwd)](const std::string& base,
      const std::string& target){
    const auto value=runtime::pathFromUtf8(target.empty()?base:target);
    if(value.is_absolute())return normalize(value);
    const auto from=runtime::pathFromUtf8(base);
    const auto root=from.is_absolute()?from:runtime::pathFromUtf8(cwd())/from;
    return normalize(root/value);
  };
  path.homedir=std::move(home);
  path.normalizeNFC=value::normalizeNFC;
  return path;
}
}
