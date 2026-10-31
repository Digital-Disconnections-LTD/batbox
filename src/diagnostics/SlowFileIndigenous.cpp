#include "diagnostics/SlowFileIndigenous.hpp"
#include "runtime/IndigenousPath.hpp"
#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
#include <sys/stat.h>
#else
#include <unistd.h>
#endif
#include <stdexcept>
#include <string>
#include <system_error>

namespace batbox::diagnostics {
namespace {
int openIndigenous(const std::string& path,int flags,int mode){
#ifdef _WIN32
  return ::_wopen(runtime::pathFromUtf8(path).c_str(),flags|_O_BINARY,mode);
#else
  return ::open(path.c_str(),flags,mode);
#endif
}
int closeIndigenous(int fd){
#ifdef _WIN32
  return ::_close(fd);
#else
  return ::close(fd);
#endif
}
int flushIndigenous(int fd){
#ifdef _WIN32
  return ::_commit(fd);
#else
  return ::fsync(fd);
#endif
}
std::string encoding(const std::optional<Json>& options){
  if(!options||options->is_null())return "utf8";
  if(options->is_string())return options->get<std::string>();
  if(options->is_object()){
    const auto item=options->find("encoding");
    if(item!=options->end()&&!item->is_null()){
      if(!item->is_string())throw std::invalid_argument("Invalid encoding");
      return item->get<std::string>();
    }
  }
  return "utf8";
}
void requireUtf8(const std::string& value){
  if(value!="utf8"&&value!="utf-8")
    throw std::invalid_argument("Unsupported encoding: "+value);
}
int mode(const std::optional<Json>& options){
  if(options&&options->is_number_integer())return options->get<int>();
  if(options&&options->is_object()){
    const auto item=options->find("mode");
    if(item!=options->end()&&item->is_number_integer())return item->get<int>();
  }
  return 0666;
}
int flags(const std::string& value){
  if(value=="w")return O_WRONLY|O_CREAT|O_TRUNC;
  if(value=="wx"||value=="xw")return O_WRONLY|O_CREAT|O_EXCL;
  if(value=="a")return O_WRONLY|O_CREAT|O_APPEND;
  if(value=="ax"||value=="xa")return O_WRONLY|O_CREAT|O_EXCL|O_APPEND;
  if(value=="r+")return O_RDWR;
  if(value=="w+")return O_RDWR|O_CREAT|O_TRUNC;
  if(value=="a+")return O_RDWR|O_CREAT|O_APPEND;
  throw std::invalid_argument("Unsupported file flag: "+value);
}
void writeAll(int fd,const SlowWriteData& data){
  const char* bytes=nullptr;
  std::size_t length=0;
  if(std::holds_alternative<std::string>(data)){
    const auto& text=std::get<std::string>(data);
    bytes=text.data();length=text.size();
  }else{
    const auto& blob=std::get<std::vector<unsigned char>>(data);
    bytes=reinterpret_cast<const char*>(blob.data());length=blob.size();
  }
  std::size_t written=0;
  while(written<length){
#ifdef _WIN32
    const auto count=::_write(fd,bytes+written,static_cast<unsigned int>(
      std::min<std::size_t>(length-written,0x7fffffff)));
#else
    const auto count=::write(fd,bytes+written,length-written);
#endif
    if(count<0){if(errno==EINTR)continue;
      throw std::system_error(errno,std::generic_category(),"write");}
    if(count==0)throw std::runtime_error("write returned zero bytes");
    written+=static_cast<std::size_t>(count);
  }
}
}

void bindIndigenousSlowFileOperations(SlowOperationImports& imports){
  imports.openSync=[](const std::string& path,const std::optional<Json>& requestedMode){
    const int fd=openIndigenous(path,flags("w"),mode(requestedMode));
    if(fd<0)throw std::system_error(errno,std::generic_category(),"open");
    return fd;
  };
  imports.writeFdSync=[](int fd,const SlowWriteData& data,
      const std::optional<Json>& requestedEncoding){
    if(std::holds_alternative<std::string>(data))
      requireUtf8(encoding(requestedEncoding));
    writeAll(fd,data);
  };
  imports.fsyncSync=[](int fd){
    if(flushIndigenous(fd)<0)throw std::system_error(errno,std::generic_category(),"fsync");
  };
  imports.closeSync=[](int fd){
    if(closeIndigenous(fd)<0)throw std::system_error(errno,std::generic_category(),"close");
  };
  imports.writePathSync=[](const std::string& path,const SlowWriteData& data,
      const std::optional<Json>& options){
    if(std::holds_alternative<std::string>(data))requireUtf8(encoding(options));
    std::string requestedFlag="w";
    if(options&&options->is_object()){
      const auto item=options->find("flag");
      if(item!=options->end()&&!item->is_null()){
        if(!item->is_string())throw std::invalid_argument("Invalid file flag");
        requestedFlag=item->get<std::string>();
      }
    }
    const int fd=openIndigenous(path,flags(requestedFlag),mode(options));
    if(fd<0)throw std::system_error(errno,std::generic_category(),"open");
    try{writeAll(fd,data);}
    catch(...){const auto original=std::current_exception();closeIndigenous(fd);
      std::rethrow_exception(original);}
    if(closeIndigenous(fd)<0)throw std::system_error(errno,std::generic_category(),"close");
  };
}
}
