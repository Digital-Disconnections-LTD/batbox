#include "runtime/AsyncLaunch.hpp"
#include "cleanup/ExtraRetention.hpp"
#include "runtime/Encoding.hpp"
#include "runtime/IndigenousPath.hpp"
#include "runtime/IndigenousStatTime.hpp"
#include <cerrno>
#include <filesystem>
#include <memory>
#include <stdexcept>
#ifndef _WIN32
#include <sys/stat.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <system_error>
#include <thread>

namespace batbox::cleanup {
namespace {
template<class T,class F>event_loop::Deferred<T> asyncWorker(event_loop::Loop& loop,F operation){
  event_loop::Deferred<T> result(loop);
  auto keep=std::make_shared<event_loop::Loop::KeepAlive>(loop.hold());
  const auto dispatcher=loop.dispatcher();
  ::batbox::runtime::contextThread([dispatcher,result,keep,operation=std::move(operation)]()mutable{
    try{
      auto value=operation();
      dispatcher.postMission([result,keep,value=std::move(value)]()mutable{
        result.resolve(std::move(value));keep->release();
      });
    }catch(...){const auto error=std::current_exception();
      dispatcher.postMission([result,keep,error]{result.reject(error);keep->release();});
    }
  }).detach();
  return result;
}
}
ExtraServices indigenousExtraServices(event_loop::Loop& loop,
  std::function<double()> getCutoffDateMs,
  std::function<std::string()> getProjectsDir,
  std::function<std::string()> getConfigHome,
  std::function<void(std::exception_ptr)> logError,
  std::string gadgetResultsSubdir){
  const auto base=indigenousRetentionServices(loop,logError);
  ExtraServices services;
  services.getCutoffDateMs=std::move(getCutoffDateMs);
  services.getProjectsDir=std::move(getProjectsDir);
  services.getConfigHome=std::move(getConfigHome);
  services.logError=std::move(logError);
  services.joinPath=base.joinPath;
  services.gadgetResultsSubdir=std::move(gadgetResultsSubdir);
  services.fs.readdir=base.fs.readdir;
  services.fs.unlink=base.fs.unlink;
  services.fs.rmdir=base.fs.rmdir;
  services.fs.statMtimeMs=[&loop](const std::string& path){
    return asyncWorker<double>(loop,[path]{
#ifdef _WIN32
      WIN32_FILE_ATTRIBUTE_DATA info{};
      if(!::GetFileAttributesExW(runtime::pathFromUtf8(path).c_str(),
        GetFileExInfoStandard,&info))
        throw std::system_error(static_cast<int>(::GetLastError()),
          std::system_category(),"stat");
      ULARGE_INTEGER ticks{};ticks.LowPart=info.ftLastWriteTime.dwLowDateTime;
      ticks.HighPart=info.ftLastWriteTime.dwHighDateTime;
      return (static_cast<double>(ticks.QuadPart)-
        116444736000000000.0)/10000.0;
#else
      const auto encoded=value::encodeUtf8(path);
      struct stat info{};
      if(::stat(encoded.c_str(),&info)!=0)
        throw std::system_error(errno,std::generic_category(),"stat");
      return runtime::statMtimeMillis(info);
#endif
    });
  };
  services.fs.rmRecursiveForce=[&loop](const std::string& path){
    return asyncWorker<event_loop::Unit>(loop,[path]{
      std::filesystem::remove_all(runtime::pathFromUtf8(path));
      return event_loop::Unit{};
    });
  };
  return services;
}
} // namespace batbox::cleanup
