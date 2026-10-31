#include "runtime/AsyncLaunch.hpp"
#include "settings/watcher/IndigenousWatcher.hpp"
#include "runtime/IndigenousPath.hpp"
#include "runtime/IndigenousStatTime.hpp"
#include <uv.h>
#ifndef _WIN32
#include <sys/stat.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <chrono>
#include <algorithm>
#include <filesystem>
#include <future>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace batbox::settings::watcher {
namespace {
using Clock=std::chrono::steady_clock;
struct Stamp {
  std::uintmax_t inode=0,size=0;
  std::int64_t modifiedSeconds=0,modifiedNanoseconds=0;
  bool operator==(const Stamp&)const=default;
};
#ifdef _WIN32
std::optional<BY_HANDLE_FILE_INFORMATION> fileInfo(const std::string& path){
  HANDLE file=::CreateFileW(runtime::pathFromUtf8(path).c_str(),FILE_READ_ATTRIBUTES,
    FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,
    FILE_FLAG_BACKUP_SEMANTICS,nullptr);
  if(file==INVALID_HANDLE_VALUE)return std::nullopt;
  BY_HANDLE_FILE_INFORMATION info{};
  const bool okay=::GetFileInformationByHandle(file,&info)!=0;
  ::CloseHandle(file);
  if(!okay)return std::nullopt;
  return info;
}
std::uintmax_t fileIdentity(const BY_HANDLE_FILE_INFORMATION& info){
  return (static_cast<std::uintmax_t>(info.dwVolumeSerialNumber)<<32)^(
    (static_cast<std::uintmax_t>(info.nFileIndexHigh)<<32)|info.nFileIndexLow);
}
#endif
std::optional<Stamp> stamp(const std::string& path){
#ifdef _WIN32
  const auto info=fileInfo(path);
  if(!info||info->dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)return std::nullopt;
  const auto ticks=(static_cast<std::uint64_t>(info->ftLastWriteTime.dwHighDateTime)<<32)|
    info->ftLastWriteTime.dwLowDateTime;
  const auto unixTicks=static_cast<std::int64_t>(ticks)-116444736000000000LL;
  return Stamp{fileIdentity(*info),
    (static_cast<std::uintmax_t>(info->nFileSizeHigh)<<32)|info->nFileSizeLow,
    unixTicks/10000000,(unixTicks%10000000)*100};
#else
  struct stat info{};
  if(::stat(path.c_str(),&info)!=0||!S_ISREG(info.st_mode))return std::nullopt;
  return Stamp{static_cast<std::uintmax_t>(info.st_ino),
    static_cast<std::uintmax_t>(info.st_size),
    runtime::statMtimeSeconds(info),runtime::statMtimeNanoseconds(info)};
#endif
}
std::optional<std::uintmax_t> directoryInode(const std::string& path){
#ifdef _WIN32
  const auto info=fileInfo(path);
  if(!info||!(info->dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))return std::nullopt;
  return fileIdentity(*info);
#else
  struct stat info{};
  if(::stat(path.c_str(),&info)!=0||!S_ISDIR(info.st_mode))
    return std::nullopt;
  return static_cast<std::uintmax_t>(info.st_ino);
#endif
}
double elapsedMs(Clock::time_point from,Clock::time_point to){
  return std::chrono::duration<double,std::milli>(to-from).count();
}
void check(int code,const char* operation){
  if(code<0)throw std::runtime_error(std::string(operation)+": "+
    uv_strerror(code));
}
}
struct IndigenousWatcher::Worker {
  struct Delivery {std::atomic_bool closed=false;Events events;};
  struct WatchHandle {
    uv_fs_event_t uv{};
    Worker* owner=nullptr;
    std::string dir;
    std::uintmax_t inode=0;
    bool active=false;
  };
  struct Pending {Stamp value;Clock::time_point lastChange;bool add=false;};
  persistence::WatchTargets targets;
  event_loop::Loop::Dispatcher owner;
  std::shared_ptr<Delivery> delivery;
  Options options;
  std::atomic_bool* closed=nullptr;
  uv_loop_t loop{};
  uv_async_t stop{};
  uv_timer_t tick{};
  std::vector<std::unique_ptr<WatchHandle>> handles;
  std::map<std::string,Stamp> live;
  std::map<std::string,Pending> pending;
  std::map<std::string,Clock::time_point> deleting;
  Clock::time_point lastPendingPoll=Clock::now();
  bool closing=false,loopInitialized=false,stopInitialized=false,
    tickInitialized=false;
  Worker(persistence::WatchTargets t,event_loop::Loop::Dispatcher d,
      Events e,Options o,std::atomic_bool* flag):targets(std::move(t)),
      owner(std::move(d)),delivery(std::make_shared<Delivery>()),
      options(o),closed(flag){delivery->events=std::move(e);}
  void emit(const std::string& kind,const std::string& path){
    if(closed->load())return;
    try{
      owner.postMission([delivery=delivery,kind,path](){
        if(delivery->closed.load())return;
        const auto& events=delivery->events;
        if(kind=="add"&&events.add)events.add(path);
        else if(kind=="change"&&events.change)events.change(path);
        else if(kind=="unlink"&&events.unlink)events.unlink(path);
        else if(kind=="error"&&events.error)events.error(path);
      });
    }catch(const std::logic_error&){}
  }
  std::map<std::string,Stamp> scan(){
    std::map<std::string,Stamp> found;
    for(const auto& dir:targets.dirs){
      std::error_code error;
      for(std::filesystem::directory_iterator it(runtime::pathFromUtf8(dir),
            std::filesystem::directory_options::skip_permission_denied,error),
          end;it!=end&&!error;it.increment(error)){
        const auto path=runtime::pathToUtf8(it->path());
        std::error_code statusError;
        const auto status=it->status(statusError);
        if(statusError)continue;
        persistence::WatchStat kind{
          std::filesystem::is_regular_file(status),
          std::filesystem::is_directory(status)};
        if(persistence::ignoreWatchPath(targets,path,kind))continue;
        if(!kind.file)continue;
        if(const auto value=stamp(path))found.emplace(path,*value);
      }
    }
    return found;
  }
  void observe(const std::map<std::string,Stamp>& found,Clock::time_point now){
    for(const auto& [path,value]:found){
      if(auto gone=deleting.find(path);gone!=deleting.end()){
        deleting.erase(gone);
        pending[path]=Pending{value,now,false};
        continue;
      }
      if(auto wait=pending.find(path);wait!=pending.end()){
        if(wait->second.value!=value){wait->second.value=value;
          wait->second.lastChange=now;}
        continue;
      }
      const auto old=live.find(path);
      if(old==live.end())pending[path]=Pending{value,now,true};
      else if(old->second!=value)pending[path]=Pending{value,now,false};
    }
    for(auto it=pending.begin();it!=pending.end();){
      if(found.contains(it->first)){++it;continue;}
      if(!it->second.add&&live.contains(it->first))
        deleting[it->first]=now;
      it=pending.erase(it);
    }
    for(const auto& [path,value]:live){
      (void)value;
      if(!found.contains(path)&&!deleting.contains(path))
        deleting[path]=now;
    }
  }
  void process(bool scanAll){
    const auto now=Clock::now();
    const bool shouldPoll=(!pending.empty()||!deleting.empty())&&
      elapsedMs(lastPendingPoll,now)>=options.pollIntervalMs;
    if(scanAll||shouldPoll){
      observe(scan(),now);
      lastPendingPoll=now;
    }
    for(auto it=deleting.begin();it!=deleting.end();){
      if(elapsedMs(it->second,now)<options.atomicWindowMs){++it;continue;}
      emit("unlink",it->first);live.erase(it->first);
      it=deleting.erase(it);
    }
    for(auto it=pending.begin();it!=pending.end();){
      if(elapsedMs(it->second.lastChange,now)<options.stabilityThresholdMs){
        ++it;continue;
      }
      emit(it->second.add?"add":"change",it->first);
      live[it->first]=it->second.value;
      it=pending.erase(it);
    }
  }
  void tryStart(WatchHandle& handle){
    const auto inode=directoryInode(handle.dir);
    if(!inode)return;
    const int code=uv_fs_event_start(&handle.uv,
      [](uv_fs_event_t* fs,const char*,int,int status){
        auto* self=static_cast<WatchHandle*>(fs->data)->owner;
        if(status<0)self->emit("error",uv_strerror(status));
        else self->process(true);
      },handle.dir.c_str(),0);
    if(code==0){handle.active=true;handle.inode=*inode;return;}
    if(code!=UV_EACCES&&code!=UV_EPERM&&code!=UV_ENOENT)
      emit("error","uv_fs_event_start: "+std::string(uv_strerror(code)));
  }
  void refreshDirectories(){
    bool changed=false;
    for(auto& handle:handles){
      const auto inode=directoryInode(handle->dir);
      if(handle->active&&(!inode||*inode!=handle->inode)){
        uv_fs_event_stop(&handle->uv);handle->active=false;
        changed=true;
      }
      if(!handle->active&&inode){
        tryStart(*handle);
        if(handle->active)changed=true;
      }
    }
    if(changed)process(true);
  }
  void shutdown(){
    if(closing)return;
    closing=true;
    for(auto& handle:handles){
      if(handle->active)uv_fs_event_stop(&handle->uv);
      uv_close(reinterpret_cast<uv_handle_t*>(&handle->uv),nullptr);
    }
    if(tickInitialized){
      uv_timer_stop(&tick);
      uv_close(reinterpret_cast<uv_handle_t*>(&tick),nullptr);
    }
    if(stopInitialized)
      uv_close(reinterpret_cast<uv_handle_t*>(&stop),nullptr);
  }
  void run(std::promise<void> ready){
    try{
      check(uv_loop_init(&loop),"uv_loop_init");
      loopInitialized=true;
      check(uv_async_init(&loop,&stop,[](uv_async_t* handle){
        static_cast<Worker*>(handle->data)->shutdown();
      }),"uv_async_init");
      stopInitialized=true;
      stop.data=this;
      check(uv_timer_init(&loop,&tick),"uv_timer_init");
      tickInitialized=true;
      tick.data=this;
      live=scan();
      for(const auto& dir:targets.dirs){
        auto handle=std::make_unique<WatchHandle>();
        handle->owner=this;handle->dir=dir;handle->uv.data=handle.get();
        check(uv_fs_event_init(&loop,&handle->uv),"uv_fs_event_init");
        tryStart(*handle);
        handles.push_back(std::move(handle));
      }
      const auto interval=static_cast<std::uint64_t>(std::max(1.0,
        options.atomicWindowMs>0?
          std::min(options.pollIntervalMs,options.atomicWindowMs):
          options.pollIntervalMs));
      check(uv_timer_start(&tick,[](uv_timer_t* timer){
        auto* self=static_cast<Worker*>(timer->data);
        self->refreshDirectories();
        self->process(false);
      },interval,interval),
        "uv_timer_start");
      ready.set_value();
      uv_run(&loop,UV_RUN_DEFAULT);
      while(uv_loop_close(&loop)==UV_EBUSY)uv_run(&loop,UV_RUN_NOWAIT);
    }catch(...){
      try{ready.set_exception(std::current_exception());}catch(...){}
      if(loopInitialized){
        shutdown();
        while(uv_loop_close(&loop)==UV_EBUSY)uv_run(&loop,UV_RUN_NOWAIT);
      }
    }
  }
};
IndigenousWatcher::IndigenousWatcher(persistence::WatchTargets targets,
    event_loop::Loop::Dispatcher owner,Events events,Options options){
  if(!owner.valid())throw std::invalid_argument("Missing settings watcher owner loop");
  if(options.stabilityThresholdMs<=0||options.pollIntervalMs<=0||
      options.pollIntervalMs>=options.stabilityThresholdMs||
      options.atomicWindowMs<0)
    throw std::invalid_argument("Invalid settings watcher timing");
  worker_=std::make_unique<Worker>(std::move(targets),std::move(owner),
    std::move(events),options,&closed_);
  std::promise<void> ready;
  auto future=ready.get_future();
  thread_=::batbox::runtime::contextThread([this,ready=std::move(ready)]()mutable{
    worker_->run(std::move(ready));
  });
  try{future.get();}catch(...){
    closed_=true;
    worker_->delivery->closed=true;
    if(thread_.joinable())thread_.join();
    throw;
  }
}
IndigenousWatcher::~IndigenousWatcher(){close();}
void IndigenousWatcher::close(){
  if(closed_.exchange(true))return;
  worker_->delivery->closed=true;
  if(thread_.joinable()){
    uv_async_send(&worker_->stop);
    thread_.join();
  }
}
}
