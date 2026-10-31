#include "runtime/AsyncLaunch.hpp"
#include "instructions/hooks/runtime/FlightEnvironment.hpp"
#include "runtime/Encoding.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/IndigenousPath.hpp"
#include <algorithm>
#include <cerrno>
#ifndef _WIN32
#include <dirent.h>
#include <unistd.h>
#else
#include <io.h>
#endif
#include <fcntl.h>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <thread>

namespace batbox::instructions::hooks::runtime {
namespace {
#ifdef _WIN32
int openIndigenous(const std::string& path,int flags,int mode=0666){
  return ::_wopen(::batbox::runtime::pathFromUtf8(path).c_str(),
    flags|_O_BINARY|_O_NOINHERIT,mode);
}
int readIndigenous(int fd,char* bytes,std::size_t count){
  return ::_read(fd,bytes,static_cast<unsigned int>(
    std::min<std::size_t>(count,0x7fffffff)));
}
int writeIndigenous(int fd,const char* bytes,std::size_t count){
  return ::_write(fd,bytes,static_cast<unsigned int>(
    std::min<std::size_t>(count,0x7fffffff)));
}
int closeIndigenous(int fd){return ::_close(fd);}
#else
int openIndigenous(const std::string& path,int flags,int mode=0666){return ::open(path.c_str(),flags,mode);}
ssize_t readIndigenous(int fd,char* bytes,std::size_t count){return ::read(fd,bytes,count);}
ssize_t writeIndigenous(int fd,const char* bytes,std::size_t count){return ::write(fd,bytes,count);}
int closeIndigenous(int fd){return ::close(fd);}
#endif
template<class T,class Work>
event_loop::Deferred<T> worker(event_loop::Loop& loop,Work work){
  event_loop::Deferred<T> output(loop);
  auto keep=std::make_shared<event_loop::Loop::KeepAlive>(loop.hold());
  const auto dispatcher=loop.dispatcher();
  try{
    ::batbox::runtime::contextThread([output,keep,dispatcher,work=std::move(work)]()mutable{
      try{
        auto value=work();
        dispatcher.postMission([output,keep,value=std::move(value)]()mutable{
          output.resolve(std::move(value));keep->release();});
      }catch(...){
        const auto error=std::current_exception();
        try{dispatcher.postMission([output,keep,error]{
          output.reject(error);keep->release();});}catch(...){keep->release();}
      }
    }).detach();
  }catch(...){output.reject(std::current_exception());keep->release();}
  return output;
}
event_loop::Unit makeDirectory(const std::string& path){
  std::error_code error;
  std::filesystem::create_directories(::batbox::runtime::pathFromUtf8(path),error);
  if(error)throw std::system_error(error,"mkdir");
  return {};
}
std::vector<std::string> listDirectory(const std::string& path){
#ifdef _WIN32
  std::vector<std::string> names;
  for(const auto& entry:std::filesystem::directory_iterator(
      ::batbox::runtime::pathFromUtf8(path)))
    names.push_back(value::decodeUtf8(
      ::batbox::runtime::pathToUtf8(entry.path().filename()),false));
  return names;
#else
  const auto encoded=value::encodeUtf8(path);
  DIR* directory=::opendir(encoded.c_str());
  if(!directory)throw std::system_error(errno,std::generic_category(),"readdir");
  try{
    std::vector<std::string> names;
    for(;;){
      errno=0;auto* entry=::readdir(directory);
      if(!entry){
        if(errno)throw std::system_error(errno,std::generic_category(),"readdir");
        break;
      }
      const auto name=value::decodeUtf8(entry->d_name,false);
      if(name!="."&&name!="..")names.push_back(name);
    }
    ::closedir(directory);return names;
  }catch(...){::closedir(directory);throw;}
#endif
}
std::string readText(const std::string& path){
  const auto encoded=value::encodeUtf8(path);
  const int fd=openIndigenous(encoded,O_RDONLY
#ifndef _WIN32
    |O_CLOEXEC
#endif
    );
  if(fd<0)throw std::system_error(errno,std::generic_category(),"readFile");
  try{
    std::string bytes;char buffer[65536];
    for(;;){
      const auto count=readIndigenous(fd,buffer,sizeof(buffer));
      if(count<0){
        if(errno==EINTR)continue;
        throw std::system_error(errno,std::generic_category(),"readFile");
      }
      if(count==0)break;
      bytes.append(buffer,static_cast<std::size_t>(count));
    }
    if(closeIndigenous(fd)<0)throw std::system_error(errno,std::generic_category(),"close");
    return value::decodeUtf8(bytes,false);
  }catch(...){closeIndigenous(fd);throw;}
}
event_loop::Unit writeText(const std::string& path,const std::string& value){
  const auto encoded=value::encodeUtf8(path);
  const int fd=openIndigenous(encoded,O_WRONLY|O_CREAT|O_TRUNC
#ifndef _WIN32
    |O_CLOEXEC
#endif
    ,0666);
  if(fd<0)throw std::system_error(errno,std::generic_category(),"writeFile");
  try{
    const auto bytes=value::encodeUtf8(value);
    std::size_t offset=0;
    while(offset<bytes.size()){
      const auto amount=writeIndigenous(fd,bytes.data()+offset,bytes.size()-offset);
      if(amount<0){
        if(errno==EINTR)continue;
        throw std::system_error(errno,std::generic_category(),"writeFile");
      }
      if(amount==0)throw std::runtime_error("writeFile returned zero bytes");
      offset+=static_cast<std::size_t>(amount);
    }
    if(closeIndigenous(fd)<0)throw std::system_error(errno,std::generic_category(),"close");
    return {};
  }catch(...){closeIndigenous(fd);throw;}
}
std::optional<std::string> errnoCode(std::exception_ptr error){
  try{if(error)std::rethrow_exception(error);}
  catch(const std::system_error& value){
    if(value.code().default_error_condition()==std::errc::no_such_file_or_directory)
      return "ENOENT";
    return "ERRNO_"+std::to_string(value.code().value());
  }catch(...){}
  return std::nullopt;
}
std::string errorMessage(std::exception_ptr error){
  try{if(error)std::rethrow_exception(error);}
  catch(const std::exception& value){return value.what();}
  catch(...){return "unknown error";}
  return "";
}
}
FlightEnvironmentImports indigenousFlightEnvironmentImports(event_loop::Loop& loop,
  std::function<std::string()> configHomeDir,
  std::function<std::string()> flightId,
  std::function<std::string()> platform,
  std::function<std::optional<std::string>()> parentEnvFile,
  std::function<void(const std::string&)> logDebug){
  FlightEnvironmentImports result;
  result.getConfigHomeDir=std::move(configHomeDir);
  result.getFlightId=std::move(flightId);
  result.getPlatform=std::move(platform);
  result.getParentEnvFile=std::move(parentEnvFile);
  result.logDebug=std::move(logDebug);
  result.mkdir=[&loop](const std::string& path){
    return worker<event_loop::Unit>(loop,[path]{return makeDirectory(path);});};
  result.readdir=[&loop](const std::string& path){
    return worker<std::vector<std::string>>(loop,[path]{return listDirectory(path);});};
  result.readFile=[&loop](const std::string& path){
    return worker<std::string>(loop,[path]{return readText(path);});};
  result.writeFile=[&loop](const std::string& path,const std::string& value){
    return worker<event_loop::Unit>(loop,[path,value]{return writeText(path,value);});};
  result.errnoCode=errnoCode;
  result.errorMessage=errorMessage;
  return result;
}
} // namespace batbox::instructions::hooks::runtime
