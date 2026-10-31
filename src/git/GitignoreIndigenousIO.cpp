#include "runtime/AsyncLaunch.hpp"
#include "git/GitignoreUpdate.hpp"
#include "runtime/Encoding.hpp"
#include "runtime/IndigenousPath.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <fcntl.h>
#ifndef _WIN32
#include <pwd.h>
#include <unistd.h>
#else
#include <io.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <stdexcept>
#include <system_error>
#include <thread>

namespace batbox::git {
namespace {
using event_loop::Deferred;
using event_loop::Unit;
#ifdef _WIN32
int openIndigenous(const std::string& path,int flags,int mode=0666){
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
int openIndigenous(const std::string& path,int flags,int mode=0666){return ::open(path.c_str(),flags,mode);}
ssize_t readIndigenous(int fd,char* bytes,std::size_t count){return ::read(fd,bytes,count);}
ssize_t writeIndigenous(int fd,const char* bytes,std::size_t count){return ::write(fd,bytes,count);}
int closeIndigenous(int fd){return ::close(fd);}
#endif
class File {
public:
  explicit File(int fd):fd_(fd){}
  ~File(){if(fd_>=0)closeIndigenous(fd_);}
  File(const File&)=delete;
  File& operator=(const File&)=delete;
  int get()const{return fd_;}
private:int fd_;
};
void writeAll(int fd,const std::string& text){
  std::size_t offset=0;
  while(offset<text.size()){
    const auto count=writeIndigenous(fd,text.data()+offset,text.size()-offset);
    if(count<0){if(errno==EINTR)continue;
      throw std::system_error(errno,std::generic_category(),"write");}
    offset+=static_cast<std::size_t>(count);
  }
}
template<class T,class F>Deferred<T> runAsync(event_loop::Loop& loop,F work){
  Deferred<T> result(loop);
  auto keep=std::make_shared<event_loop::Loop::KeepAlive>(loop.hold());
  const auto owner=loop.dispatcher();
  ::batbox::runtime::contextThread([result,keep,owner,work=std::move(work)]()mutable{
    try{
      T value=work();
      owner.postMission([result,keep,value=std::move(value)]()mutable{
        result.resolve(std::move(value));keep->release();
      });
    }catch(...){
      const auto error=std::current_exception();
      try{owner.postMission([result,keep,error]{
        result.reject(error);keep->release();
      });}catch(const std::logic_error&){}
    }
  }).detach();
  return result;
}
}
std::string indigenousHomedir(){
#ifdef _WIN32
  const DWORD size=::GetEnvironmentVariableW(L"USERPROFILE",nullptr,0);
  if(!size)throw std::system_error(static_cast<int>(::GetLastError()),
    std::system_category(),"homedir");
  std::wstring home(size,L'\0');
  const DWORD copied=::GetEnvironmentVariableW(L"USERPROFILE",home.data(),size);
  if(!copied||copied>=size)throw std::system_error(static_cast<int>(::GetLastError()),
    std::system_category(),"homedir");
  home.resize(copied);
  return runtime::pathToUtf8(std::filesystem::path(home));
#else
  if(const char* home=std::getenv("HOME");home&&*home)return home;
  long count=::sysconf(_SC_GETPW_R_SIZE_MAX);
  if(count<1024)count=16384;
  std::string buffer(static_cast<std::size_t>(count),'\0');
  struct passwd record{},*found=nullptr;
  const int code=::getpwuid_r(::getuid(),&record,buffer.data(),
    buffer.size(),&found);
  if(code||!found||!record.pw_dir)
    throw std::system_error(code?code:ENOENT,std::generic_category(),
      "homedir");
  return record.pw_dir;
#endif
}
GitignoreIO indigenousGitignoreIO(event_loop::Loop& loop){
  GitignoreIO io;
  io.mkdirRecursive=[&loop](const std::string& path){
    const auto encoded=value::encodeUtf8(path);
    return runAsync<Unit>(loop,[encoded]{
      std::error_code error;
      std::filesystem::create_directories(runtime::pathFromUtf8(encoded),error);
      if(error)throw std::system_error(error,"mkdir");
      return Unit{};
    });
  };
  io.readUtf8=[&loop](const std::string& path){
    const auto encoded=value::encodeUtf8(path);
    return runAsync<std::string>(loop,[encoded]{
      File file(openIndigenous(encoded,O_RDONLY
#ifndef _WIN32
        |O_CLOEXEC
#endif
        ));
      if(file.get()<0)throw std::system_error(errno,std::generic_category(),
        "readFile");
      std::string text;
      char chunk[8192];
      for(;;){
        const auto count=readIndigenous(file.get(),chunk,sizeof(chunk));
        if(count==0)break;
        if(count<0){if(errno==EINTR)continue;
          throw std::system_error(errno,std::generic_category(),"readFile");}
        text.append(chunk,static_cast<std::size_t>(count));
      }
      return text;
    });
  };
  io.appendUtf8=[&loop](const std::string& path,const std::string& text){
    const auto encodedPath=value::encodeUtf8(path);
    const auto encodedText=value::encodeUtf8(text);
    return runAsync<Unit>(loop,[encodedPath,encodedText]{
      File file(openIndigenous(encodedPath,
        O_WRONLY|O_APPEND|O_CREAT
#ifndef _WIN32
        |O_CLOEXEC
#endif
        ,0666));
      if(file.get()<0)throw std::system_error(errno,std::generic_category(),
        "appendFile");
      writeAll(file.get(),encodedText);
      return Unit{};
    });
  };
  io.writeUtf8=[&loop](const std::string& path,const std::string& text){
    const auto encodedPath=value::encodeUtf8(path);
    const auto encodedText=value::encodeUtf8(text);
    return runAsync<Unit>(loop,[encodedPath,encodedText]{
      File file(openIndigenous(encodedPath,
        O_WRONLY|O_TRUNC|O_CREAT
#ifndef _WIN32
        |O_CLOEXEC
#endif
        ,0666));
      if(file.get()<0)throw std::system_error(errno,std::generic_category(),
        "writeFile");
      writeAll(file.get(),encodedText);
      return Unit{};
    });
  };
  return io;
}
}
