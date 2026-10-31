#include "runtime/AsyncLaunch.hpp"
#include "gadget_results/Storage.hpp"
#include "runtime/Encoding.hpp"
#include "runtime/IndigenousPath.hpp"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <filesystem>
#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#else
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <thread>
#include <uv.h>

namespace batbox::gadget_results {
namespace {
std::string errorCode(int code){
  switch(code){
    case ENOENT:return "ENOENT";case EACCES:return "EACCES";case ENOSPC:return "ENOSPC";
    case EROFS:return "EROFS";case EMFILE:return "EMFILE";case EEXIST:return "EEXIST";
    case ENOTDIR:return "ENOTDIR";case EISDIR:return "EISDIR";case ENAMETOOLONG:return "ENAMETOOLONG";
    case ELOOP:return "ELOOP";case EINVAL:return "EINVAL";case EIO:return "EIO";
    default:return "ERRNO_"+std::to_string(code);
  }
}
FileSystemError fileError(const std::string& operation,const std::string& path,int code){
  const auto name=errorCode(code);
#ifdef _WIN32
  const auto message=std::generic_category().message(code);
#else
  const auto message=std::string(uv_strerror(uv_translate_sys_error(code)));
#endif
  return FileSystemError(name+": "+message+", "+operation+" '"+path+"'",name,path);
}
void writeFile(const std::string& path,const std::string& text){
  // Node writeFile's default mode is 0666 filtered by the process umask.
  const auto bytes=value::encodeUtf8(text);
#ifdef _WIN32
  const auto indigenous=runtime::pathFromUtf8(path);
  HANDLE file=::CreateFileW(indigenous.c_str(),GENERIC_WRITE,
    FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,CREATE_NEW,
    FILE_ATTRIBUTE_NORMAL,nullptr);
  if(file==INVALID_HANDLE_VALUE){
    const auto condition=std::error_code(static_cast<int>(::GetLastError()),
      std::system_category()).default_error_condition();
    throw fileError("open",path,condition.value());
  }
  std::size_t offset=0;
  while(offset<bytes.size()){
    const auto wanted=static_cast<DWORD>(std::min<std::size_t>(bytes.size()-offset,0xffffffffu));
    DWORD count=0;
    if(!::WriteFile(file,bytes.data()+offset,wanted,&count,nullptr)){
      const auto condition=std::error_code(static_cast<int>(::GetLastError()),
        std::system_category()).default_error_condition();
      ::CloseHandle(file);throw fileError("write",path,condition.value());
    }
    if(count==0){::CloseHandle(file);throw fileError("write",path,EIO);}
    offset+=count;
  }
  if(!::CloseHandle(file))throw fileError("close",path,EIO);
#else
  const auto filePath=value::encodeUtf8(path);
  const int fd=::open(filePath.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0666);
  if(fd<0)throw fileError("open",path,errno);
  std::size_t offset=0;
  while(offset<bytes.size()){
    const auto count=::write(fd,bytes.data()+offset,bytes.size()-offset);
    if(count<0){const int code=errno;if(code==EINTR)continue;::close(fd);throw fileError("write",path,code);}
    if(count==0){::close(fd);throw fileError("write",path,EIO);}
    offset+=static_cast<std::size_t>(count);
  }
  if(::close(fd)<0)throw fileError("close",path,errno);
#endif
}
template<class Operation>Completion dispatch(event_loop::Loop& loop,Operation operation){
  Completion completed(loop);auto keep=std::make_shared<event_loop::Loop::KeepAlive>(loop.hold());
  const auto dispatcher=loop.dispatcher();
  try{
    ::batbox::runtime::contextThread([operation=std::move(operation),completed,dispatcher,keep]{
      std::exception_ptr error;try{operation();}catch(...){error=std::current_exception();}
      try{dispatcher.postMission([completed,error,keep]{
        if(error)completed.reject(error);else completed.resolve({});keep->release();
      });}catch(...){keep->release();}
    }).detach();
  }catch(...){keep->release();completed.reject(std::current_exception());}
  return completed;
}
}
StorageImports indigenousStorageImports(event_loop::Loop& loop,StorageImports imports){
  imports.mkdir=[&loop](const auto& path){return dispatch(loop,[path]{
    std::error_code error;std::filesystem::create_directories(runtime::pathFromUtf8(path),error);
    if(error)throw fileError("mkdir",path,error.value());
  });};
  imports.writeExclusive=[&loop](const auto& path,const auto& text){
    return dispatch(loop,[path,text]{writeFile(path,text);});
  };
  return imports;
}
} // namespace batbox::gadget_results
