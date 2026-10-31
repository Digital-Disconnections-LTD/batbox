#include "locking/FileOps.hpp"
#include "runtime/Encoding.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/IndigenousPath.hpp"
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fcntl.h>
#include <stdexcept>
#include <system_error>
#if !defined(_WIN32)
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace batbox::locking::file_ops {
namespace {
[[noreturn]] void osError(const char* action,int code=errno){
  throw std::system_error(code,std::generic_category(),action);
}
Acquired attempt(const std::string& file,const Options& options,
    std::optional<Precision> stashedPrecision,bool skipStale){
  const auto directory=lockPath(file,options);
  const auto bytes=value::encodeUtf8(directory);
#if defined(_WIN32)
  std::error_code createError;
  const bool created=std::filesystem::create_directory(
    runtime::pathFromUtf8(bytes),createError);
  if(createError&&createError!=std::errc::file_exists)
    throw std::filesystem::filesystem_error("acquire directory lock",
      runtime::pathFromUtf8(bytes),createError);
#else
  const bool created=::mkdir(bytes.c_str(),0777)==0;
#endif
  if(created){
    try{
      double mtime=0;
      Precision precision=Precision::Milliseconds;
      if(stashedPrecision){
        mtime=statMtime(directory);precision=*stashedPrecision;
      }else{
        mtime=std::ceil(nowMs()/1000)*1000+5;
        setMtime(directory,mtime);
        mtime=statMtime(directory);
        precision=std::fmod(mtime,1000)==0
          ?Precision::Seconds:Precision::Milliseconds;
      }
      return {file,directory,mtime,precision};
    }catch(...){
      try{remove(directory);}catch(...){}
      throw;
    }
  }
#if !defined(_WIN32)
  if(errno!=EEXIST)osError("acquire directory lock");
#endif
  if(skipStale)throw Error("ELOCKED","Lock file is already being held",file);
  double mtime=0;
  try{mtime=statMtime(directory);}
  catch(const std::system_error& error){
    if(error.code()==std::errc::no_such_file_or_directory)
      return attempt(file,options,stashedPrecision,true);
    throw;
  }
  if(!(mtime<nowMs()-options.staleMs))
    throw Error("ELOCKED","Lock file is already being held",file);
  remove(directory);
  return attempt(file,options,stashedPrecision,true);
}
}
Options normalize(Options options){
  options.staleMs=std::max(options.staleMs==0?0:options.staleMs,2000.0);
  double update=options.updateMs?*options.updateMs:options.staleMs/2;
  options.updateMs=std::max(std::min(update,options.staleMs/2),1000.0);
  return options;
}
std::string canonical(const std::string& file,bool realpath){
  const auto input=runtime::pathFromUtf8(value::encodeUtf8(file));
  const auto path=realpath?std::filesystem::canonical(input):
    std::filesystem::absolute(input).lexically_normal();
  auto result=value::decodeUtf8(runtime::pathToUtf8(path));
  while(result.size()>1&&result.back()=='/')result.pop_back();
  return result;
}
std::string lockPath(const std::string& file,const Options& options){
  return options.lockfilePath?*options.lockfilePath:file+".lock";
}
double nowMs(){
  return static_cast<double>(std::chrono::duration_cast<
    std::chrono::milliseconds>(std::chrono::system_clock::now()
      .time_since_epoch()).count());
}
double statMtime(const std::string& path){
#if defined(_WIN32)
  const auto indigenous=runtime::pathFromUtf8(value::encodeUtf8(path));
  const auto modified=std::filesystem::last_write_time(indigenous);
  const auto current=std::filesystem::file_time_type::clock::now();
  const auto unixNow=std::chrono::system_clock::now();
  return static_cast<double>(std::chrono::duration_cast<
    std::chrono::milliseconds>((modified-current+unixNow).time_since_epoch()).count());
#else
  struct stat status{};
  if(::stat(value::encodeUtf8(path).c_str(),&status)!=0)
    osError("stat directory lock");
#if defined(__APPLE__)
  return static_cast<double>(status.st_mtimespec.tv_sec)*1000+
    std::floor(static_cast<double>(status.st_mtimespec.tv_nsec)/1000000);
#else
  return static_cast<double>(status.st_mtim.tv_sec)*1000+
    std::floor(static_cast<double>(status.st_mtim.tv_nsec)/1000000);
#endif
#endif
}
double heartbeatMtime(Precision precision){
  const auto now=nowMs();
  return precision==Precision::Seconds?std::ceil(now/1000)*1000:now;
}
void setMtime(const std::string& path,double mtimeMs){
#if defined(_WIN32)
  const auto when=std::chrono::system_clock::time_point(
    std::chrono::milliseconds(static_cast<std::int64_t>(mtimeMs)));
  const auto now=std::chrono::system_clock::now();
  const auto indigenousNow=std::filesystem::file_time_type::clock::now();
  std::filesystem::last_write_time(runtime::pathFromUtf8(value::encodeUtf8(path)),
    std::chrono::time_point_cast<std::filesystem::file_time_type::duration>(
      indigenousNow+(when-now)));
#else
  const auto whole=static_cast<std::int64_t>(mtimeMs);
  timespec stamp{};
  stamp.tv_sec=whole/1000;
  stamp.tv_nsec=(whole%1000)*1000000;
  const timespec times[2]={stamp,stamp};
  if(::utimensat(AT_FDCWD,value::encodeUtf8(path).c_str(),times,0)!=0)
    osError("update directory lock mtime");
#endif
}
void remove(const std::string& path){
#if defined(_WIN32)
  const auto indigenous=runtime::pathFromUtf8(value::encodeUtf8(path));
  if(std::filesystem::exists(indigenous)&&!std::filesystem::is_directory(indigenous))
    throw std::system_error(std::make_error_code(std::errc::not_a_directory),
      "remove directory lock");
  (void)std::filesystem::remove(indigenous);
#else
  if(::rmdir(value::encodeUtf8(path).c_str())!=0&&errno!=ENOENT)
    osError("remove directory lock");
#endif
}
Acquired acquire(const std::string& file,const Options& options,
    std::optional<Precision> stashedPrecision){
  return attempt(file,options,stashedPrecision,false);
}
bool check(const std::string& file,const Options& options){
  try{return !(statMtime(lockPath(file,options))<nowMs()-options.staleMs);}
  catch(const std::system_error& error){
    if(error.code()==std::errc::no_such_file_or_directory)return false;
    throw;
  }
}
}
