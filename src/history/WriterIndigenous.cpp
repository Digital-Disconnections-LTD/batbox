#include "history/WriterIndigenous.hpp"
#include "runtime/Encoding.hpp"
#include "runtime/IndigenousPath.hpp"
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <filesystem>
#ifndef _WIN32
#include <fcntl.h>
#include <stdexcept>
#include <system_error>
#include <unistd.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace batbox::history {
namespace {
[[noreturn]] void fail(const char* operation){
#ifdef _WIN32
  throw std::system_error(static_cast<int>(::GetLastError()),std::system_category(),operation);
#else
  throw std::system_error(errno,std::generic_category(),operation);
#endif
}
void append(const std::string& path,const std::string& text,int mode){
  const auto bytes=value::encodeUtf8(text);
#ifdef _WIN32
  (void)mode;
  const auto indigenousPath=runtime::pathFromUtf8(path);
  HANDLE file=::CreateFileW(indigenousPath.c_str(),FILE_APPEND_DATA,
    FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(file==INVALID_HANDLE_VALUE)fail("append history");
  try{
    for(std::size_t offset=0;offset<bytes.size();){
      const auto chunk=static_cast<DWORD>(std::min<std::size_t>(bytes.size()-offset,0xffffffffu));
      DWORD written=0;
      if(!::WriteFile(file,bytes.data()+offset,chunk,&written,nullptr))fail("append history");
      if(written==0)throw std::runtime_error("Short history write");
      offset+=written;
    }
  }catch(...){::CloseHandle(file);throw;}
  if(!::CloseHandle(file))fail("close history");
#else
  const auto indigenousPath=value::encodeUtf8(path);
  const int descriptor=::open(indigenousPath.c_str(),O_WRONLY|O_CREAT|O_APPEND,mode);
  if(descriptor<0)fail("append history");
  try{
    for(std::size_t offset=0;offset<bytes.size();){
      const auto count=::write(descriptor,bytes.data()+offset,bytes.size()-offset);
      if(count<0){if(errno==EINTR)continue;fail("append history");}
      if(count==0)throw std::runtime_error("Short history write");
      offset+=static_cast<std::size_t>(count);
    }
  }catch(...){(void)::close(descriptor);throw;}
  if(::close(descriptor)<0)fail("close history");
#endif
}
}
HistoryWriterServices bindIndigenousHistoryWriterServices(
    HistoryWriterServices services,
    std::shared_ptr<fs_operations::IndigenousAsyncFsHost> host,
    locking::Manager& locks,cleanup::Roster& cleanup){
  if(!services.loop||!host)
    throw std::invalid_argument("History owner loop and filesystem host required");
  auto& loop=*services.loop;
  services.join=[](const std::string& left,const std::string& right){
    return runtime::pathToUtf8((runtime::pathFromUtf8(left)/runtime::pathFromUtf8(right))
      .lexically_normal());};
  services.writeFile=[host](const std::string& path,const std::string& text,
      const Json& options){
    return host->submit<event_loop::Unit>([path,text,options]{
      if(options.value("flag",std::string{})!="a"||
          options.value("encoding",std::string{})!="utf8")
        throw std::invalid_argument("History write options unsupported");
      append(path,text,options.value("mode",0600));return event_loop::Unit{};
    });
  };
  services.appendFile=[host](const std::string& path,const std::string& text,
      const Json& options){
    return host->submit<event_loop::Unit>([path,text,options]{
      append(path,text,options.value("mode",0600));return event_loop::Unit{};
    });
  };
  services.lock=[&locks](const std::string& path,const Json& options){
    locking::Options indigenous;
    indigenous.staleMs=options.at("stale").get<double>();
    const auto& retry=options.at("retries");
    indigenous.retry.retries=retry.at("retries").get<int>();
    indigenous.retry.minTimeoutMs=retry.at("minTimeout").get<double>();
    return locks.lock(path,std::move(indigenous));
  };
  services.sleep=[&loop](double ms){
    event_loop::Deferred<event_loop::Unit> result(loop);
    loop.setTimeout(ms,[result]{result.resolve({});});
    return result;
  };
  services.nowMs=[]{
    return static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count());};
  services.registerCleanup=[&cleanup](std::function<event_loop::Deferred<event_loop::Unit>()> fn){
    cleanup.registerCleanup(std::move(fn));};
  services.errorToString=[](const std::exception_ptr& error){
    try{std::rethrow_exception(error);}
    catch(const std::exception& value){return std::string("Error: ")+value.what();}
    catch(...){return std::string("Error");}
  };
  return services;
}
}
