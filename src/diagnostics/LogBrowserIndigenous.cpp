#include "diagnostics/LogBrowserIndigenous.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/IndigenousPath.hpp"
#include "runtime/IndigenousStatTime.hpp"
#include <cerrno>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <system_error>
#ifndef _WIN32
#include <sys/stat.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <utility>

namespace batbox::diagnostics {
IndigenousLogBrowserIO::IndigenousLogBrowserIO(event_loop::Loop& loop,unsigned workers):loop_(loop){
  if(workers==0)throw std::invalid_argument("At least one file worker is required");
  workers_.reserve(workers);
  for(unsigned i=0;i<workers;++i)workers_.emplace_back([this]{work();});
}
IndigenousLogBrowserIO::~IndigenousLogBrowserIO(){
  {std::lock_guard lock(mutex_);stopping_=true;}
  ready_.notify_all();
  for(auto& worker:workers_)worker.join();
}
void IndigenousLogBrowserIO::submit(std::function<void()> job){
  {std::lock_guard lock(mutex_);
    if(stopping_)throw std::logic_error("Log file workers are stopped");
    jobs_.push_back(std::move(job));}
  ready_.notify_one();
}
void IndigenousLogBrowserIO::work(){
  for(;;){
    std::function<void()> job;
    {std::unique_lock lock(mutex_);
      ready_.wait(lock,[this]{return stopping_||!jobs_.empty();});
      if(jobs_.empty())return;
      job=std::move(jobs_.front());jobs_.pop_front();}
    job();
  }
}
event_loop::Deferred<std::vector<std::string>>
IndigenousLogBrowserIO::readdir(const std::string& path){
  event_loop::Deferred<std::vector<std::string>> result(loop_);
  submit([result,path]{
    try{
      std::vector<std::string> names;
      for(const auto& entry:std::filesystem::directory_iterator(runtime::pathFromUtf8(path)))
        names.push_back(runtime::pathToUtf8(entry.path().filename()));
      result.resolve(std::move(names));
    }catch(...){result.reject(std::current_exception());}
  });
  return result;
}
event_loop::Deferred<std::string>
IndigenousLogBrowserIO::readFile(const std::string& path){
  event_loop::Deferred<std::string> result(loop_);
  submit([result,path]{
    try{
      std::ifstream file(runtime::pathFromUtf8(path),std::ios::binary);
      if(!file)throw std::system_error(errno,std::generic_category(),"readFile");
      const std::string raw(std::istreambuf_iterator<char>(file),{});
      if(file.bad())throw std::system_error(errno,std::generic_category(),"readFile");
      result.resolve(value::decodeUtf8(raw));
    }catch(...){result.reject(std::current_exception());}
  });
  return result;
}
event_loop::Deferred<double>
IndigenousLogBrowserIO::statMtime(const std::string& path){
  event_loop::Deferred<double> result(loop_);
  submit([result,path]{
    try{
#ifdef _WIN32
      WIN32_FILE_ATTRIBUTE_DATA info{};
      if(!::GetFileAttributesExW(runtime::pathFromUtf8(path).c_str(),
        GetFileExInfoStandard,&info))
        throw std::system_error(static_cast<int>(::GetLastError()),
          std::system_category(),"stat");
      ULARGE_INTEGER ticks{};ticks.LowPart=info.ftLastWriteTime.dwLowDateTime;
      ticks.HighPart=info.ftLastWriteTime.dwHighDateTime;
      const double milliseconds=(static_cast<double>(ticks.QuadPart)-
        116444736000000000.0)/10000.0;
#else
      struct stat info{};
      if(::stat(path.c_str(),&info)<0)
        throw std::system_error(errno,std::generic_category(),"stat");
      const double milliseconds=runtime::statMtimeMillis(info);
#endif
      result.resolve(std::trunc(milliseconds));
    }catch(...){result.reject(std::current_exception());}
  });
  return result;
}

LogBrowserServices indigenousLogBrowserServices(event_loop::Loop& loop,
    std::shared_ptr<IndigenousLogBrowserIO> io,
    std::function<std::string()> errorsDirectory,
    std::function<std::string(const std::string&,const std::string&)> join,
    std::function<Json(const std::string&)> jsonParse,
    std::function<void(const std::string&)> logError){
  if(!io)throw std::invalid_argument("Log file workers are required");
  LogBrowserServices services;
  services.loop=&loop;
  services.errorsDirectory=std::move(errorsDirectory);
  services.readdir=[io](const std::string& path){return io->readdir(path);};
  services.readFile=[io](const std::string& path){return io->readFile(path);};
  services.statMtime=[io](const std::string& path){return io->statMtime(path);};
  services.join=std::move(join);
  services.jsonParse=std::move(jsonParse);
  services.logError=std::move(logError);
  return services;
}
}
