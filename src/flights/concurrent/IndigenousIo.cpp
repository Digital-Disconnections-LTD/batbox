#include "flights/concurrent/IndigenousIo.hpp"
#include "flights/IndigenousPath.hpp"
#include "runtime/Encoding.hpp"
#include <cerrno>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fcntl.h>
#include <fstream>
#include <limits>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#else
#include <dirent.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <stdexcept>
#include <system_error>

namespace batbox::flights::concurrent {
namespace {
[[noreturn]] void fail(const char* operation){throw std::system_error(errno,std::generic_category(),operation);}
std::string read(const std::string& path){
#if defined(_WIN32)
  std::ifstream input(indigenousPath(value::encodeUtf8(path)),std::ios::binary);
  if(!input)fail("read session");
  const std::string bytes((std::istreambuf_iterator<char>(input)),{});
  if(input.bad())fail("read session");
  return value::decodeUtf8(bytes);
#else
  const int fd=::open(value::encodeUtf8(path).c_str(),O_RDONLY);
  if(fd<0)fail("read session");
  std::string bytes;char buffer[8192];
  try{for(;;){
    const auto n=::read(fd,buffer,sizeof(buffer));
    if(n<0){if(errno==EINTR)continue;fail("read session");}
    if(!n)break;
    bytes.append(buffer,static_cast<std::size_t>(n));
  }}catch(...){(void)::close(fd);throw;}
  if(::close(fd))fail("close session");
  return value::decodeUtf8(bytes);
#endif
}
void write(const std::string& path,const std::string& text){
  const auto bytes=value::encodeUtf8(text);
#if defined(_WIN32)
  std::ofstream output(indigenousPath(path),std::ios::binary|std::ios::trunc);
  if(!output)fail("write session");
  output.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));
  output.close();if(!output)fail("write session");
#else
  const int fd=::open(value::encodeUtf8(path).c_str(),O_WRONLY|O_CREAT|O_TRUNC,0666);
  if(fd<0)fail("write session");
  try{for(std::size_t offset=0;offset<bytes.size();){
    const auto n=::write(fd,bytes.data()+offset,bytes.size()-offset);
    if(n<0){if(errno==EINTR)continue;fail("write session");}
    if(!n)throw std::runtime_error("Short session write");
    offset+=static_cast<std::size_t>(n);
  }}catch(...){(void)::close(fd);throw;}
  if(::close(fd))fail("close session");
#endif
}
}
Services bindIndigenousIo(Services s,std::shared_ptr<fs_operations::IndigenousAsyncFsHost> host,
    cleanup::Roster& cleanup){
  if(!host||!s.loop)throw std::invalid_argument("Session registry loop and filesystem host required");
  const auto fs=fs_operations::indigenousNodeFsAsyncImports(*s.loop,host);
  s.mkdir=[fs](const std::string& path,bool recursive,int mode){
    return fs.mkdir(path,Json{{"recursive",recursive},{"mode",mode}});
  };
  s.chmod=[host](const std::string& path,int mode){return host->submit<Unit>([path,mode]{
#if defined(_WIN32)
    if(::_wchmod(indigenousPath(path).c_str(),mode))
      fail("chmod session directory");
#else
    if(::chmod(value::encodeUtf8(path).c_str(),mode))fail("chmod session directory");
#endif
    return Unit{};
  });};
  s.writeFile=[host](const std::string& path,const std::string& text){
    return host->submit<Unit>([path,text]{write(path,text);return Unit{};});
  };
  s.readFile=[host](const std::string& path){return host->submit<std::string>([path]{return read(path);});};
  s.unlink=fs.unlink;
  s.readdir=[host](const std::string& path){return host->submit<std::vector<std::string>>([path]{
#if defined(_WIN32)
    std::vector<std::string> names;
    for(const auto& entry:std::filesystem::directory_iterator(indigenousPath(path))){
      const auto bytes=entry.path().filename().u8string();
      names.emplace_back(reinterpret_cast<const char*>(bytes.data()),bytes.size());
    }
    return names;
#else
    DIR* directory=::opendir(value::encodeUtf8(path).c_str());
    if(!directory)fail("readdir sessions");
    std::vector<std::string> names;errno=0;
    while(const auto* entry=::readdir(directory)){
      const std::string name=entry->d_name;
      if(name!="."&&name!="..")names.push_back(value::decodeUtf8(name));
      errno=0;
    }
    const auto error=errno;(void)::closedir(directory);
    if(error){errno=error;fail("readdir sessions");}return names;
#endif
  });};
  s.join=[](const std::string& a,const std::string& b){
    // Roster suffixes are relative; preserve Node path.join's trailing slash.
    return runtime::pathToUtf8((indigenousPath(value::encodeUtf8(a))/
      indigenousPath(value::encodeUtf8(b))).lexically_normal());
  };
  s.jsonParse=[](const std::string& text){return value::parse(text);};
  s.jsonStringify=[](const Json& value){return value::stringify(value);};
  s.registerCleanup=[&cleanup](std::function<Deferred<Unit>()> fn){cleanup.registerCleanup(std::move(fn));};
  s.now=[](){return static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count());};
  s.isProcessRunning=[](double pid){
#if defined(_WIN32)
    if(!std::isfinite(pid)||pid<=1||pid>std::numeric_limits<DWORD>::max()||
        std::floor(pid)!=pid)return false;
    HANDLE handle=::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,
      static_cast<DWORD>(pid));
    if(!handle)return ::GetLastError()==ERROR_ACCESS_DENIED;
    DWORD exitCode=0;
    const bool active=::GetExitCodeProcess(handle,&exitCode)&&exitCode==STILL_ACTIVE;
    ::CloseHandle(handle);return active;
#else
    if(!std::isfinite(pid)||pid<=1||pid>std::numeric_limits<pid_t>::max()||std::floor(pid)!=pid)return false;
    return ::kill(static_cast<pid_t>(pid),0)==0;
#endif
  };
  s.isFsInaccessible=[](const std::exception_ptr& error){
    try{std::rethrow_exception(error);}catch(const std::system_error& e){
      const auto code=e.code();
      return code==std::errc::no_such_file_or_directory||
        code==std::errc::permission_denied||
        code==std::errc::not_a_directory||
        code==std::errc::too_many_symbolic_link_levels;
    }catch(...){return false;}
  };
  s.errorMessage=[](const std::exception_ptr& error){
    try{std::rethrow_exception(error);}catch(const std::exception& e){return std::string(e.what());}
    catch(...){return std::string("Unknown error");}
  };
  return s;
}
} // namespace batbox::flights::concurrent
