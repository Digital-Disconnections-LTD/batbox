#include "runtime/AsyncLaunch.hpp"
#include "cron/CronMissions.hpp"
#include "bootstrap/BootstrapState.hpp"
#include "runtime/Encoding.hpp"
#include "runtime/Platform.hpp"
#include "runtime/IndigenousPath.hpp"
#include <algorithm>
#include <cerrno>
#include <filesystem>
#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include <stdexcept>
#include <system_error>
#include <thread>
#include <utility>

namespace batbox::cron {
namespace {
#ifdef _WIN32
int openIndigenous(const std::string& path,int flags,int mode){
  return ::_wopen(runtime::pathFromUtf8(path).c_str(),
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
int openIndigenous(const std::string& path,int flags,int mode){return ::open(path.c_str(),flags,mode);}
ssize_t readIndigenous(int fd,char* bytes,std::size_t count){return ::read(fd,bytes,count);}
ssize_t writeIndigenous(int fd,const char* bytes,std::size_t count){return ::write(fd,bytes,count);}
int closeIndigenous(int fd){return ::close(fd);}
#endif
template<class T,class Operation>
event_loop::Deferred<T> worker(event_loop::Loop& loop,Operation operation){
  event_loop::Deferred<T> output(loop);
  auto keep=std::make_shared<event_loop::Loop::KeepAlive>(loop.hold());
  const auto dispatcher=loop.dispatcher();
  try{
    ::batbox::runtime::contextThread([output,keep,dispatcher,
      operation=std::move(operation)]()mutable{
      try{
        auto value=operation();
        dispatcher.postMission([output,keep,value=std::move(value)]()mutable{
          output.resolve(std::move(value));keep->release();
        });
      }catch(...){
        const auto error=std::current_exception();
        try{dispatcher.postMission([output,keep,error]{
          output.reject(error);keep->release();
        });}catch(...){keep->release();}
      }
    }).detach();
  }catch(...){output.reject(std::current_exception());keep->release();}
  return output;
}
std::string readText(const std::string& path){
  const auto encoded=value::encodeUtf8(path);
  const int fd=openIndigenous(encoded,O_RDONLY
#ifndef _WIN32
    |O_CLOEXEC
#endif
    ,0);
  if(fd<0)throw std::system_error(errno,std::generic_category(),
    "read cron tasks");
  try{
    std::string bytes;char buffer[65536];
    for(;;){
      const auto count=readIndigenous(fd,buffer,sizeof(buffer));
      if(count<0){
        if(errno==EINTR)continue;
        throw std::system_error(errno,std::generic_category(),
          "read cron tasks");
      }
      if(count==0)break;
      bytes.append(buffer,static_cast<std::size_t>(count));
    }
    if(closeIndigenous(fd)<0)throw std::system_error(errno,std::generic_category(),
      "close cron tasks");
    return value::decodeUtf8(bytes,false);
  }catch(...){closeIndigenous(fd);throw;}
}
event_loop::Unit makeDirectory(const std::string& path){
  std::filesystem::create_directories(runtime::pathFromUtf8(path));
  return {};
}
event_loop::Unit writeText(const std::string& path,
  const std::string& text){
  const auto encoded=value::encodeUtf8(path);
  const auto bytes=value::encodeUtf8(text);
  const int fd=openIndigenous(encoded,O_WRONLY|O_CREAT|O_TRUNC
#ifndef _WIN32
    |O_CLOEXEC
#endif
    ,
    0666);
  if(fd<0)throw std::system_error(errno,std::generic_category(),
    "write cron tasks");
  try{
    std::size_t offset=0;
    while(offset<bytes.size()){
      const auto count=writeIndigenous(fd,bytes.data()+offset,bytes.size()-offset);
      if(count<0){
        if(errno==EINTR)continue;
        throw std::system_error(errno,std::generic_category(),
          "write cron tasks");
      }
      if(count==0)throw std::runtime_error("write cron tasks returned zero");
      offset+=static_cast<std::size_t>(count);
    }
    if(closeIndigenous(fd)<0)throw std::system_error(errno,std::generic_category(),
      "close cron tasks");
    return {};
  }catch(...){closeIndigenous(fd);throw;}
}
bool inaccessible(std::exception_ptr error){
  try{if(error)std::rethrow_exception(error);}
  catch(const std::system_error& ex){
    const int value=ex.code().value();
    return value==ENOENT||value==EACCES||value==EPERM||
      value==ENOTDIR||value==ELOOP;
  }catch(...){}
  return false;
}
}
CronMissionServices makeIndigenousCronMissionServices(event_loop::Loop& loop,
  std::shared_ptr<bootstrap::BootstrapState> state,
  std::function<void(std::exception_ptr)> logError,
  std::function<void(const std::string&)> logDebug){
  if(!state||!logError||!logDebug)
    throw std::invalid_argument("Cron native state and log bindings required");
  CronMissionServices services;
  services.getProjectRoot=[state]{return state->identity.directories.projectRoot;};
  services.randomUUID=[]{return platform::randomUUID();};
  services.nowMs=[]{return static_cast<double>(platform::nowMillis());};
  services.readFile=[&loop](const std::string& path){
    return worker<std::string>(loop,[path]{return readText(path);});
  };
  services.readFileSync=readText;
  services.mkdir=[&loop](const std::string& path){
    return worker<event_loop::Unit>(loop,[path]{return makeDirectory(path);});
  };
  services.writeFile=[&loop](const std::string& path,
    const std::string& text){
    return worker<event_loop::Unit>(loop,[path,text]{
      return writeText(path,text);
    });
  };
  services.isFsInaccessible=inaccessible;
  services.logError=std::move(logError);
  services.logDebug=std::move(logDebug);
  services.addFlightCronMission=[state](
    std::shared_ptr<bootstrap::FlightCronMission> mission){
    bootstrap::addFlightCronMission(state->cron,std::move(mission));
  };
  services.removeFlightCronMissions=[state](const std::vector<std::string>& ids){
    return bootstrap::removeFlightCronMissions(state->cron,ids);
  };
  services.getFlightCronMissions=[state]{
    return bootstrap::getFlightCronMissions(state->cron);
  };
  return services;
}
}
