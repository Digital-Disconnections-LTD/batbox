#include "fs_operations/IndigenousPathChecks.hpp"
#include "runtime/Unicode.hpp"
#include <filesystem>
#include <stdexcept>
#include <string>

namespace batbox::fs_operations {
namespace {
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
std::string normalize(const std::filesystem::path& value){
  auto result=value.lexically_normal().string();
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
    for(const auto& part:parts)if(!part.empty())joined/=part;
    return normalize(joined);
  };
  path.isAbsolute=[](const std::string& value){return !value.empty()&&value[0]=='/';};
  path.resolve=[cwd=std::move(cwd)](const std::string& base,
      const std::string& target){
    const std::filesystem::path value=target.empty()?std::filesystem::path(base):
      std::filesystem::path(target);
    if(value.is_absolute())return normalize(value);
    const auto root=std::filesystem::path(base).is_absolute()?std::filesystem::path(base):
      std::filesystem::path(cwd())/base;
    return normalize(root/value);
  };
  path.homedir=std::move(home);
  path.normalizeNFC=value::normalizeNFC;
  return path;
}
}
