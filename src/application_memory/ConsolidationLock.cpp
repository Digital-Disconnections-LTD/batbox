#include "application_memory/Consolidation.hpp"
#include "runtime/IndigenousPath.hpp"
#include <uv.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <regex>

namespace batbox::application_memory {
namespace {
std::filesystem::path lockPath(const std::string& dir){
  return runtime::pathFromUtf8(dir)/".consolidate-lock";
}
double modified(const std::filesystem::path& path){
  return std::chrono::duration<double,std::milli>(
    std::chrono::file_clock::to_sys(std::filesystem::last_write_time(path)).time_since_epoch()).count();
}
std::string readText(const std::filesystem::path& path){
  std::ifstream stream(path,std::ios::binary);
  if(!stream)throw std::runtime_error("Cannot read consolidation lock");
  return std::string(std::istreambuf_iterator<char>(stream),{});
}
void writeText(const std::filesystem::path& path,const std::string& text){
  std::ofstream stream(path,std::ios::binary|std::ios::trunc);
  if(!stream)throw std::runtime_error("Cannot write consolidation lock");
  stream<<text;stream.close();
  if(!stream)throw std::runtime_error("Cannot finish consolidation lock");
}
}
double readLastConsolidatedAt(const std::string& directory){
  try{return modified(lockPath(directory));}catch(...){return 0;}
}
std::optional<double> tryAcquireConsolidationLock(const std::string& directory,
    double nowMillis){
  const auto path=lockPath(directory);
  std::optional<double> prior;std::optional<int> holder;
  try{
    prior=modified(path);
    try{holder=std::stoi(readText(path));}catch(...){}
  }catch(...){}
  if(prior&&nowMillis-*prior<60*60*1000&&holder&&*holder>0&&
      ::uv_kill(*holder,0)==0)return {};
  std::filesystem::create_directories(path.parent_path());
  const auto pid=::uv_os_getpid();
  writeText(path,std::to_string(pid));
  try{if(std::stoi(readText(path))!=pid)return {};}
  catch(...){return {};}
  return prior.value_or(0);
}
void rollbackConsolidationLock(const std::string& directory,double prior){
  const auto path=lockPath(directory);
  if(prior==0){std::filesystem::remove(path);return;}
  writeText(path,"");
  const auto time=std::chrono::system_clock::time_point(
    std::chrono::duration_cast<std::chrono::system_clock::duration>(
      std::chrono::duration<double,std::milli>(prior)));
  std::filesystem::last_write_time(path,std::chrono::file_clock::from_sys(time));
}
std::vector<std::string> flightsTouchedSince(const std::string& directory,
    double since,const std::string& current){
  std::vector<std::string> ids;
  static const std::regex uuid("^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$");
  std::error_code error;
  if(!std::filesystem::is_directory(runtime::pathFromUtf8(directory),error))return ids;
  for(const auto& file:std::filesystem::directory_iterator(runtime::pathFromUtf8(directory))){
    if(file.path().extension()!=".jsonl")continue;
    const auto id=runtime::pathToUtf8(file.path().stem());
    if(id==current||!std::regex_match(id,uuid))continue;
    try{if(modified(file.path())>since)ids.push_back(id);}catch(...){}
  }
  return ids;
}
} // namespace batbox::application_memory
