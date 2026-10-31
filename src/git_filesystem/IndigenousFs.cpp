#include "runtime/AsyncLaunch.hpp"
#include "git_filesystem/GitFilesystem.hpp"
#include "runtime/Encoding.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/IndigenousPath.hpp"
#include "runtime/IndigenousStatTime.hpp"
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <system_error>
#ifndef _WIN32
#include <sys/stat.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <thread>
#include <utility>

namespace batbox::git_filesystem {
namespace {
template<class Result,class Operation>Deferred<Result> onWorker(event_loop::Loop& loop,
  Operation operation){
  Deferred<Result> output(loop);
  auto keep=std::make_shared<event_loop::Loop::KeepAlive>(loop.hold());
  auto dispatcher=loop.dispatcher();
  try{
    ::batbox::runtime::contextThread([output,operation=std::move(operation),keep,dispatcher]()mutable{
      std::optional<Result> result;std::exception_ptr error;
      try{result=operation();}catch(...){error=std::current_exception();}
      try{dispatcher.postMission([output,result=std::move(result),error,keep]{
        if(error)output.reject(error);else output.resolve(*result);
        keep->release();
      });}catch(...){keep->release();}
    }).detach();
  }catch(...){keep->release();output.reject(std::current_exception());}
  return output;
}
struct Snapshot {
  bool exists=false;std::uint64_t inode=0,size=0;std::int64_t seconds=0,nanos=0;
  bool operator==(const Snapshot&)const=default;
};
Snapshot snapshot(const std::string& path){
#ifdef _WIN32
  const auto indigenous=runtime::pathFromUtf8(path);
  HANDLE file=::CreateFileW(indigenous.c_str(),FILE_READ_ATTRIBUTES,
    FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,
    FILE_FLAG_BACKUP_SEMANTICS,nullptr);
  if(file==INVALID_HANDLE_VALUE)return {};
  BY_HANDLE_FILE_INFORMATION info{};
  const bool okay=::GetFileInformationByHandle(file,&info)!=0;
  ::CloseHandle(file);
  if(!okay)return {};
  const auto index=(static_cast<std::uint64_t>(info.nFileIndexHigh)<<32)|info.nFileIndexLow;
  const auto ticks=(static_cast<std::uint64_t>(info.ftLastWriteTime.dwHighDateTime)<<32)|
    info.ftLastWriteTime.dwLowDateTime;
  const auto unixTicks=static_cast<std::int64_t>(ticks)-116444736000000000LL;
  return {true,index^static_cast<std::uint64_t>(info.dwVolumeSerialNumber),
    (static_cast<std::uint64_t>(info.nFileSizeHigh)<<32)|info.nFileSizeLow,
    unixTicks/10000000,(unixTicks%10000000)*100};
#else
  struct stat info{};
  if(::stat(value::encodeUtf8(path).c_str(),&info)!=0)return {};
  return {true,static_cast<std::uint64_t>(info.st_ino),
    static_cast<std::uint64_t>(info.st_size),runtime::statMtimeSeconds(info),
    runtime::statMtimeNanoseconds(info)};
#endif
}
struct Watch {
  struct Wait {std::mutex mutex;std::condition_variable_any cv;};
  std::shared_ptr<Wait> wait=std::make_shared<Wait>();
  std::jthread worker;
};
struct Roster {
  std::map<std::string,std::shared_ptr<Watch>> watches;
  ~Roster(){watches.clear();}
};
}
Services indigenousGitFsServices(event_loop::Loop& loop){
  Services services;
  services.readFile=[&loop](const std::string& path){
    return onWorker<std::string>(loop,[path]{
      std::ifstream file(runtime::pathFromUtf8(path),std::ios::binary);
      if(!file)throw std::system_error(errno,std::generic_category(),"readFile "+path);
      std::string bytes(std::istreambuf_iterator<char>{file},{});
      if(!file.eof()&&file.fail())throw std::runtime_error("readFile failed: "+path);
      return value::decodeUtf8(bytes);
    });
  };
  services.stat=[&loop](const std::string& path){
    return onWorker<FileStat>(loop,[path]{
      std::error_code error;const auto status=std::filesystem::status(runtime::pathFromUtf8(path),error);
      if(error)throw std::system_error(error,"stat "+path);
      return FileStat{std::filesystem::is_regular_file(status),
        std::filesystem::is_directory(status)};
    });
  };
  services.readdir=[&loop](const std::string& path){
    return onWorker<std::vector<std::string>>(loop,[path]{
      std::vector<std::string> entries;
      for(const auto& entry:std::filesystem::directory_iterator(runtime::pathFromUtf8(path)))
        entries.push_back(value::decodeUtf8(runtime::pathToUtf8(entry.path().filename())));
      return entries;
    });
  };
  auto roster=std::make_shared<Roster>();
  services.watchFile=[roster,dispatcher=loop.dispatcher()](const std::string& path,
    std::uint64_t interval,std::function<void()> callback){
    if(!dispatcher.isOwnerThread())throw std::logic_error("Git file watch owner required");
    roster->watches.erase(path);
    auto watch=std::make_shared<Watch>();
    auto wait=watch->wait;
    const auto period=std::chrono::milliseconds(interval?interval:1);
    watch->worker=std::jthread([wait,path,period,callback=std::move(callback),dispatcher](
      std::stop_token stop){
      auto previous=snapshot(path);
      std::unique_lock lock(wait->mutex);
      while(!stop.stop_requested()){
        wait->cv.wait_for(lock,stop,period,[]{return false;});
        if(stop.stop_requested())break;
        lock.unlock();
        const auto current=snapshot(path);
        if(current!=previous){
          previous=current;
          try{dispatcher.postMission(callback);}catch(...){}
        }
        lock.lock();
      }
    });
    roster->watches[path]=std::move(watch);
  };
  services.unwatchFile=[roster,dispatcher=loop.dispatcher()](const std::string& path){
    if(!dispatcher.isOwnerThread())throw std::logic_error("Git unwatch owner required");
    roster->watches.erase(path);
  };
  return services;
}
} // namespace batbox::git_filesystem
