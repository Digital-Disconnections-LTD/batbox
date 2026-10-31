#include "runtime/AsyncLaunch.hpp"
#include "auth/refresh/Invalidate.hpp"
#include "runtime/Encoding.hpp"
#include "runtime/IndigenousPath.hpp"
#include <cerrno>
#include <memory>
#include <filesystem>
#include <stdexcept>
#include <system_error>
#if !defined(_WIN32)
#include <sys/stat.h>
#endif
#include <thread>

namespace batbox::auth::refresh {
event_loop::Deferred<double> indigenousStatMtimeMs(event_loop::Loop& loop,
    const std::string& path){
  event_loop::Deferred<double> output(loop);
  auto hold=std::make_shared<event_loop::Loop::KeepAlive>(loop.hold());
  output.then([hold](const double*,const std::exception_ptr&){
    hold->release();
  });
  try{
    ::batbox::runtime::contextThread([output,path,hold]{
      try{
#if defined(_WIN32)
        const auto modified=std::filesystem::last_write_time(
          runtime::pathFromUtf8(value::encodeUtf8(path)));
        const auto current=std::filesystem::file_time_type::clock::now();
        const auto now=std::chrono::system_clock::now();
        output.resolve(static_cast<double>(std::chrono::duration_cast<
          std::chrono::milliseconds>((modified-current+now).time_since_epoch()).count()));
#else
        struct stat result{};
        if(::stat(value::encodeUtf8(path).c_str(),&result)!=0)
          throw std::system_error(errno,std::generic_category(),
            "stat OAuth credentials");
#if defined(__APPLE__)
        const auto seconds=result.st_mtimespec.tv_sec;
        const auto nanoseconds=result.st_mtimespec.tv_nsec;
#else
        const auto seconds=result.st_mtim.tv_sec;
        const auto nanoseconds=result.st_mtim.tv_nsec;
#endif
        output.resolve(static_cast<double>(seconds)*1000+
          static_cast<double>(nanoseconds)/1000000);
#endif
      }catch(...){output.reject(std::current_exception());}
    }).detach();
  }catch(...){hold->release();output.reject(std::current_exception());}
  return output;
}
}
