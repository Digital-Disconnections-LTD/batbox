#include "runtime/AsyncLaunch.hpp"
#include "settings/remote/IndigenousFile.hpp"
#include "runtime/Encoding.hpp"
#include "runtime/IndigenousPath.hpp"
#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include <memory>
#include <stdexcept>
#include <system_error>
#include <thread>

namespace batbox::settings::remote {
namespace {
int closeIndigenous(int fd){
#ifdef _WIN32
  return ::_close(fd);
#else
  return ::close(fd);
#endif
}
[[noreturn]] void osError(const char* action){
  throw std::system_error(errno,std::generic_category(),action);
}
struct File {
  int fd=-1;
  explicit File(const std::string& path){
#ifdef _WIN32
    fd=::_wopen(runtime::pathFromUtf8(path).c_str(),
      O_WRONLY|O_CREAT|O_TRUNC|_O_BINARY,0600);
#else
    fd=::open(path.c_str(),O_WRONLY|O_CREAT|O_TRUNC,0600);
#endif
    if(fd<0)osError("open remote settings");
  }
  ~File(){if(fd>=0)closeIndigenous(fd);}
  void writeAll(const std::string& bytes){
    std::size_t offset=0;
    while(offset<bytes.size()){
#ifdef _WIN32
      const auto count=::_write(fd,bytes.data()+offset,
        static_cast<unsigned int>(std::min<std::size_t>(bytes.size()-offset,0x7fffffff)));
#else
      const auto count=::write(fd,bytes.data()+offset,bytes.size()-offset);
#endif
      if(count<0){if(errno==EINTR)continue;osError("write remote settings");}
      if(count==0)throw std::runtime_error("Short remote settings write");
      offset+=static_cast<std::size_t>(count);
    }
#ifdef _WIN32
    if(::_commit(fd)!=0)osError("datasync remote settings");
#elif defined(__APPLE__)
    if(::fsync(fd)!=0)osError("datasync remote settings");
#else
    if(::fdatasync(fd)!=0)osError("datasync remote settings");
#endif
    if(closeIndigenous(fd)!=0){fd=-1;osError("close remote settings");}
    fd=-1;
  }
};
event_loop::Deferred<event_loop::Unit> runFileJob(event_loop::Loop& loop,
    std::function<void()> job){
  event_loop::Deferred<event_loop::Unit> output(loop);
  auto hold=std::make_shared<event_loop::Loop::KeepAlive>(loop.hold());
  output.then([hold](const event_loop::Unit*,const std::exception_ptr&){
    hold->release();
  });
  try{
    ::batbox::runtime::contextThread([output,job=std::move(job),hold](){
      try{job();output.resolve({});}
      catch(...){output.reject(std::current_exception());}
    }).detach();
  }catch(...){hold->release();output.reject(std::current_exception());}
  return output;
}
}
event_loop::Deferred<event_loop::Unit> saveSettings(
    event_loop::Loop& loop,const Json& settings,
    const std::function<std::string()>& getSettingsPath,
    const std::function<void(const std::string&)>& debug){
  if(!getSettingsPath||!debug)
    throw std::invalid_argument("Missing remote settings file binding");
  event_loop::Deferred<event_loop::Unit> output(loop);
  try{
    const auto path=getSettingsPath();
    const auto bytes=value::encodeUtf8(value::stringify(settings,2));
    runFileJob(loop,[path,bytes](){File file(path);file.writeAll(bytes);})
      .then([output,path,debug](const event_loop::Unit*,
          const std::exception_ptr& error){
        try{
          if(error){
            try{std::rethrow_exception(error);}
            catch(const std::exception& caught){
              debug("Remote settings: Failed to save - "+
                std::string(caught.what()));}
            catch(...){debug("Remote settings: Failed to save - unknown error");}
          }else debug("Remote settings: Saved to "+path);
          output.resolve({});
        }catch(...){output.reject(std::current_exception());}
      });
  }catch(const std::exception& error){
    try{debug("Remote settings: Failed to save - "+
      std::string(error.what()));output.resolve({});}
    catch(...){output.reject(std::current_exception());}
  }catch(...){
    try{debug("Remote settings: Failed to save - unknown error");
      output.resolve({});}
    catch(...){output.reject(std::current_exception());}
  }
  return output;
}
event_loop::Deferred<event_loop::Unit> unlinkSettings(
    event_loop::Loop& loop,const std::string& path){
  return runFileJob(loop,[path](){
#ifdef _WIN32
    if(::_wunlink(runtime::pathFromUtf8(path).c_str())!=0)osError("unlink remote settings");
#else
    if(::unlink(path.c_str())!=0)osError("unlink remote settings");
#endif
  });
}
}
