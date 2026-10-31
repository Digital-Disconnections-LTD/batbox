#include "flights/IndigenousPath.hpp"
#include "flights/async_queue/Queue.hpp"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#if !defined(_WIN32)
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <utility>

namespace batbox::flights::async_queue {
namespace {
std::runtime_error systemError(const char* action,const std::string& path,int code){
  return std::runtime_error(std::string(action)+" '"+path+"': "+std::strerror(code));
}
void appendFile(const std::string& path,const std::string& data){
#if defined(_WIN32)
  std::ofstream output(indigenousPath(path),std::ios::binary|std::ios::app);
  if(!output)throw systemError("open",path,errno);
  output.write(data.data(),static_cast<std::streamsize>(data.size()));
  output.close();
  if(!output)throw systemError("append",path,errno?errno:EIO);
#else
  const int fd=::open(path.c_str(),O_WRONLY|O_CREAT|O_APPEND|O_CLOEXEC,0600);
  if(fd<0)throw systemError("open",path,errno);
  std::size_t offset=0;
  while(offset<data.size()){
    const auto count=::write(fd,data.data()+offset,data.size()-offset);
    if(count<0){
      const int code=errno;if(code==EINTR)continue;
      ::close(fd);throw systemError("append",path,code);
    }
    if(count==0){::close(fd);throw std::runtime_error("append returned zero bytes: "+path);}
    offset+=static_cast<std::size_t>(count);
  }
  if(::close(fd)<0)throw systemError("close",path,errno);
#endif
}
void ensureDirectory(const std::string& path){
  if(path.empty())return;
#if defined(_WIN32)
  std::filesystem::create_directories(indigenousPath(path));
#else
  std::string current;
  if(path.front()=='/')current="/";
  std::size_t start=path.front()=='/'?1:0;
  for(std::size_t i=start;i<=path.size();++i){
    if(i<path.size()&&path[i]!='/')continue;
    if(i==start){start=i+1;continue;}
    if(!current.empty()&&current.back()!='/')current+='/';
    current.append(path,start,i-start);start=i+1;
    if(::mkdir(current.c_str(),0700)<0){
      const int code=errno;
      if(code!=EEXIST)throw systemError("mkdir",current,code);
      struct stat info{};
      if(::stat(current.c_str(),&info)<0||!S_ISDIR(info.st_mode))
        throw systemError("mkdir",current,errno==0?ENOTDIR:errno);
    }
  }
#endif
}
template<class Operation>Completion dispatchFileOperation(event_loop::Loop& loop,Operation operation){
  Completion completed(loop);
  try{
    // The work request itself keeps the loop alive. Reuse libuv's pool rather
    // than creating one thread and one uv_async handle per filesystem batch.
    loop.queueWork(std::move(operation),[completed](std::exception_ptr failure){
      if(failure)completed.reject(failure);else completed.resolve({});
    });
  }catch(...){completed.reject(std::current_exception());}
  return completed;
}
}
QueueImports indigenousQueueImports(event_loop::Loop& loop,
  std::function<void(std::exception_ptr)> onBackgroundError){
  QueueImports imports;
  imports.appendFile=[&loop](const std::string& path,const std::string& data){
    return dispatchFileOperation(loop,[path,data]{appendFile(path,data);});
  };
  imports.mkdir=[&loop](const std::string& path){
    return dispatchFileOperation(loop,[path]{ensureDirectory(path);});
  };
  imports.onBackgroundError=std::move(onBackgroundError);
  return imports;
}
} // namespace batbox::flights::async_queue
