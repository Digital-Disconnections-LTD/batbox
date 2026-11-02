#include "poetryEntries/SystemPoetryFile.hpp"
#include "poetryEntries/SystemHead.hpp"
#include "runtime/IndigenousPath.hpp"
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iterator>

namespace batbox::poetryEntries {
namespace {
namespace fs=std::filesystem;
const std::string& builtInHead(){
  static const std::string head=prepareSystemHead(builtInSystemPoetrySource());
  return head;
}
SystemPoetrySource builtIn(fs::path path){
  SystemPoetrySource source;
  source.path=std::move(path);
  source.head=builtInHead();
  source.bytes=builtInSystemPoetrySource().size();
  const auto scan=scanPlaceholders(source.head);
  source.placeholders=scan.known;
  source.unknown=scan.unknown;
  return source;
}
std::string stamp(const fs::path& path){
  if(path.empty())return "none";
  std::error_code error;
  const auto time=fs::last_write_time(path,error);
  if(error)return "missing";
  const auto size=fs::file_size(path,error);
  const auto ticks=time.time_since_epoch();
  const auto seconds=std::chrono::duration_cast<std::chrono::seconds>(ticks);
  return std::to_string(seconds.count())+":"+
    std::to_string(static_cast<long long>((ticks-seconds).count()))+":"+
    (error?std::string("unknown"):std::to_string(size));
}
std::string plural(std::uintmax_t count,const char* one,const char* many){
  return std::to_string(count)+" "+(count==1?one:many);
}
}
fs::path systemPoetryPath(const fs::path& configDirectory){
  return configDirectory.empty()?fs::path{}:configDirectory/"system.md";
}
SystemPoetrySource loadSystemPoetry(const fs::path& configDirectory){
  const auto path=systemPoetryPath(configDirectory);
  auto fallback=builtIn(path);
  if(path.empty())return fallback;
  std::error_code error;
  const auto status=fs::status(path,error);
  if(error||!fs::exists(status))return fallback;
  if(!fs::is_regular_file(status)){fallback.problem="is not a regular file";return fallback;}
  std::ifstream input(path,std::ios::binary);
  if(!input.is_open()){fallback.problem="could not be read";return fallback;}
  std::string raw{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
  if(input.bad()){fallback.problem="could not be read";return fallback;}
  auto head=prepareSystemHead(raw);
  if(head.empty()){fallback.problem="is empty";return fallback;}
  SystemPoetrySource source;
  source.path=path;
  source.fromFile=true;
  source.bytes=raw.size();
  source.head=std::move(head);
  const auto scan=scanPlaceholders(source.head);
  source.placeholders=scan.known;
  source.unknown=scan.unknown;
  return source;
}
std::string displayPath(const fs::path& path){
  const char* home=std::getenv("HOME");
#ifdef _WIN32
  if(!home||!*home)home=std::getenv("USERPROFILE");
#endif
  const auto text=runtime::pathToUtf8(path);
  if(!home||!*home)return text;
  const auto base=runtime::pathToUtf8(runtime::pathFromUtf8(home).lexically_normal());
  const auto trimmed=base.size()>1&&(base.back()=='/'||base.back()=='\\')?
    base.substr(0,base.size()-1):base;
  if(text.size()>trimmed.size()&&text.compare(0,trimmed.size(),trimmed)==0&&
     (text[trimmed.size()]=='/'||text[trimmed.size()]=='\\'))
    return "~"+text.substr(trimmed.size());
  return text;
}
std::string describeSystemPoetry(const SystemPoetrySource& source){
  if(!source.fromFile)return "built-in";
  return displayPath(source.path)+" · "+plural(source.bytes,"byte","bytes")+" · "+
    plural(source.placeholders,"placeholder","placeholders");
}
std::vector<std::string> systemPoetryWarnings(const SystemPoetrySource& source){
  std::vector<std::string> warnings;
  if(!source.problem.empty())warnings.push_back(displayPath(source.path)+" "+
    source.problem+"; using the built-in system prompt");
  if(!source.unknown.empty()){
    std::string names;
    for(const auto& name:source.unknown){if(!names.empty())names+=", ";names+=name;}
    warnings.push_back(std::string(source.unknown.size()==1?"Unknown placeholder":
      "Unknown placeholders")+" left as text: "+names);
  }
  return warnings;
}
SystemPoetryFile::SystemPoetryFile(fs::path configDirectory)
  :configDirectory_(std::move(configDirectory)){}
bool SystemPoetryFile::refresh(){
  auto current=stamp(systemPoetryPath(configDirectory_));
  if(loaded_&&current==stamp_)return false;
  source_=loadSystemPoetry(configDirectory_);
  stamp_=std::move(current);
  loaded_=true;
  return true;
}
}
