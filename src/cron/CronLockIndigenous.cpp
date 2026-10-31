#include "runtime/AsyncLaunch.hpp"
#include "cron/CronLock.hpp"
#include "bootstrap/BootstrapState.hpp"
#include "cleanup/Roster.hpp"
#include "runtime/Encoding.hpp"
#include "runtime/Platform.hpp"
#include "runtime/IndigenousPath.hpp"
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <filesystem>
#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#include <signal.h>
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
int unlinkIndigenous(const std::string& path){
  return ::_wunlink(runtime::pathFromUtf8(path).c_str());
}
#else
int openIndigenous(const std::string& path,int flags,int mode){return ::open(path.c_str(),flags,mode);}
ssize_t readIndigenous(int fd,char* bytes,std::size_t count){return ::read(fd,bytes,count);}
ssize_t writeIndigenous(int fd,const char* bytes,std::size_t count){return ::write(fd,bytes,count);}
int closeIndigenous(int fd){return ::close(fd);}
int unlinkIndigenous(const std::string& path){return ::unlink(path.c_str());}
#endif
template<class T,class Operation>
event_loop::Deferred<T> onWorker(event_loop::Loop& loop,
  Operation operation){
  event_loop::Deferred<T> output(loop);
  auto keep=std::make_shared<event_loop::Loop::KeepAlive>(loop.hold());
  const auto owner=loop.dispatcher();
  try{
    ::batbox::runtime::contextThread([output,keep,owner,operation=std::move(operation)]()mutable{
      try{
        auto result=operation();
        owner.postMission([output,keep,result=std::move(result)]()mutable{
          output.resolve(std::move(result));keep->release();
        });
      }catch(...){
        const auto error=std::current_exception();
        try{owner.postMission([output,keep,error]{
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
    "read scheduler lock");
  std::string bytes;char buffer[8192];
  for(;;){
    const auto count=readIndigenous(fd,buffer,sizeof(buffer));
    if(count<0){
      if(errno==EINTR)continue;
      const auto error=errno;closeIndigenous(fd);
      throw std::system_error(error,std::generic_category(),
        "read scheduler lock");
    }
    if(count==0)break;
    bytes.append(buffer,static_cast<std::size_t>(count));
  }
  if(closeIndigenous(fd)<0)throw std::system_error(errno,std::generic_category(),
    "close scheduler lock");
  return value::decodeUtf8(bytes,false);
}
event_loop::Unit writeText(const std::string& path,
  const std::string& text,bool exclusive){
  const auto encoded=value::encodeUtf8(path);
  const auto bytes=value::encodeUtf8(text);
  const int flags=O_WRONLY|O_CREAT|
#ifndef _WIN32
    O_CLOEXEC|
#endif
    (exclusive?O_EXCL:O_TRUNC);
  const int fd=openIndigenous(encoded,flags,0666);
  if(fd<0)throw std::system_error(errno,std::generic_category(),
    "write scheduler lock");
  try{
    std::size_t offset=0;
    while(offset<bytes.size()){
      const auto count=writeIndigenous(fd,bytes.data()+offset,bytes.size()-offset);
      if(count<0){
        if(errno==EINTR)continue;
        throw std::system_error(errno,std::generic_category(),
          "write scheduler lock");
      }
      if(count==0)throw std::runtime_error("zero scheduler lock write");
      offset+=static_cast<std::size_t>(count);
    }
  }catch(...){closeIndigenous(fd);throw;}
  if(closeIndigenous(fd)<0)throw std::system_error(errno,std::generic_category(),
    "close scheduler lock");
  return {};
}
event_loop::Unit makeDirectory(const std::string& path){
  std::filesystem::create_directories(runtime::pathFromUtf8(path));return {};
}
event_loop::Unit unlinkFile(const std::string& path){
  const auto encoded=value::encodeUtf8(path);
  if(unlinkIndigenous(encoded)<0)
    throw std::system_error(errno,std::generic_category(),
      "unlink scheduler lock");
  return {};
}
bool processRunning(double pid){
  if(pid<=1||!std::isfinite(pid))return false;
#ifdef _WIN32
  HANDLE process=::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,
    static_cast<DWORD>(pid));
  if(!process)return false;
  DWORD code=0;const bool okay=::GetExitCodeProcess(process,&code)!=0;
  ::CloseHandle(process);
  return okay&&code==STILL_ACTIVE;
#else
  return ::kill(static_cast<pid_t>(pid),0)==0;
#endif
}
}
CronLockServices makeIndigenousCronLockServices(event_loop::Loop& loop,
  std::shared_ptr<bootstrap::BootstrapState> bootstrap,
  cleanup::Roster& roster,
  std::function<void(const std::string&)> logDebug){
  if(!bootstrap||!logDebug)
    throw std::invalid_argument("Cron lock native bindings required");
  CronLockServices services;
  services.getProjectRoot=[bootstrap]{
    return bootstrap->identity.directories.projectRoot;
  };
  services.getFlightId=[bootstrap]{return bootstrap->identity.flightId;};
  services.nowMs=[]{return static_cast<double>(platform::nowMillis());};
  services.pid=[]{
#ifdef _WIN32
    return static_cast<double>(::GetCurrentProcessId());
#else
    return static_cast<double>(::getpid());
#endif
  };
  services.isProcessRunning=processRunning;
  services.readFile=[&loop](const std::string& path){
    return onWorker<std::string>(loop,[path]{return readText(path);});
  };
  services.writeFile=[&loop](const std::string& path,
    const std::string& text,bool exclusive){
    return onWorker<event_loop::Unit>(loop,[path,text,exclusive]{
      return writeText(path,text,exclusive);
    });
  };
  services.mkdir=[&loop](const std::string& path){
    return onWorker<event_loop::Unit>(loop,[path]{
      return makeDirectory(path);
    });
  };
  services.unlink=[&loop](const std::string& path){
    return onWorker<event_loop::Unit>(loop,[path]{
      return unlinkFile(path);
    });
  };
  services.registerCleanup=[&roster](auto callback){
    return roster.registerCleanup(std::move(callback));
  };
  services.logDebug=std::move(logDebug);
  return services;
}
CronLock::CronLock(event_loop::Loop& loop,
  std::shared_ptr<bootstrap::BootstrapState> bootstrap,
  cleanup::Roster& roster,
  std::function<void(const std::string&)> logDebug):
  CronLock(loop,makeIndigenousCronLockServices(loop,std::move(bootstrap),
    roster,std::move(logDebug))){}
}
