#include "fs_operations/NodeFsBytesIndigenous.hpp"
#include "runtime/IndigenousPath.hpp"
#include <atomic>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <system_error>
#if !defined(_WIN32)
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <fcntl.h>

namespace batbox::fs_operations {
namespace {
void fail(const char* operation){
  throw std::system_error(errno,std::generic_category(),operation);
}
struct IndigenousFd {
#if defined(_WIN32)
  explicit IndigenousFd(std::filesystem::path selected):path(std::move(selected)),
      file(path,std::ios::binary){
    if(!file)fail("open");
  }
  std::filesystem::path path;
  std::ifstream file;
  std::mutex mutex;
  bool closed=false;
#else
  explicit IndigenousFd(int value):descriptor(value){}
  ~IndigenousFd(){const int value=descriptor.exchange(-1);if(value>=0)::close(value);}
  std::atomic<int> descriptor;
#endif
};
double toNumber(const Json& value){
  if(value.is_null())return 0;
  if(value.is_boolean())return value.get<bool>()?1:0;
  if(value.is_number())return value.get<double>();
  if(value.is_array()){
    if(value.empty())return 0;
    if(value.size()==1)return toNumber(value[0]);
    return std::numeric_limits<double>::quiet_NaN();
  }
  if(!value.is_string())return std::numeric_limits<double>::quiet_NaN();
  const auto string=value.get<std::string>();
  const char* begin=string.c_str();
  char* end=nullptr;
  errno=0;
  const double number=std::strtod(begin,&end);
  if(begin==end){
    for(char c:string)if(c!=' '&&c!='\t'&&c!='\n'&&c!='\r')
      return std::numeric_limits<double>::quiet_NaN();
    return 0;
  }
  while(*end==' '||*end=='\t'||*end=='\n'||*end=='\r')++end;
  return *end?std::numeric_limits<double>::quiet_NaN():number;
}
std::shared_ptr<ByteBuffer> allocUnsafe(double count){
  if(!std::isfinite(count)||count<0||
      count>static_cast<double>(std::numeric_limits<std::size_t>::max()))
    throw std::range_error("Invalid buffer size");
  return std::make_shared<ByteBuffer>(static_cast<std::size_t>(count));
}
}
NodeFsBytesImports indigenousNodeFsBytesImports(event_loop::Loop& loop,
    std::shared_ptr<IndigenousAsyncFsHost> host){
  if(!host)throw std::invalid_argument("Filesystem worker host is required");
  NodeFsBytesImports imports;
  imports.loop=&loop;
  imports.toNumber=toNumber;
  imports.allocUnsafe=allocUnsafe;
  imports.readFile=[host](const std::string& path){
    return host->submit<ByteBuffer>([path]{
      std::ifstream file(runtime::pathFromUtf8(path),std::ios::binary);
      if(!file)fail("readFile");
      const std::string bytes(std::istreambuf_iterator<char>(file),{});
      if(file.bad())fail("readFile");
      return ByteBuffer(bytes.begin(),bytes.end());
    });
  };
  imports.open=[host](const std::string& path,const std::string& mode){
    return host->submit<std::shared_ptr<AsyncByteHandle>>([host,path,mode]{
      if(mode!="r")throw std::invalid_argument("Unsupported file open mode");
#if defined(_WIN32)
      auto indigenous=std::make_shared<IndigenousFd>(runtime::pathFromUtf8(path));
#else
      const int descriptor=::open(path.c_str(),O_RDONLY|O_CLOEXEC);
      if(descriptor<0)fail("open");
      auto indigenous=std::make_shared<IndigenousFd>(descriptor);
#endif
      auto handle=std::make_shared<AsyncByteHandle>();
      handle->stat=[host,indigenous]{
        return host->submit<Json>([indigenous]{
#if defined(_WIN32)
          std::lock_guard lock(indigenous->mutex);
          if(indigenous->closed)throw std::runtime_error("fstat: file is closed");
          return Json::object({{"size",std::filesystem::file_size(indigenous->path)}});
#else
          struct stat info{};
          if(::fstat(indigenous->descriptor.load(),&info)<0)fail("fstat");
          return Json::object({{"size",info.st_size}});
#endif
        });
      };
      handle->read=[host,indigenous](const std::shared_ptr<ByteBuffer>& buffer,
          std::size_t offset,std::size_t length,std::size_t position){
        return host->submit<Json>([indigenous,buffer,offset,length,position]{
          if(!buffer||offset>buffer->size()||length>buffer->size()-offset)
            throw std::out_of_range("Imported read exceeds buffer");
#if defined(_WIN32)
          std::lock_guard lock(indigenous->mutex);
          if(indigenous->closed)throw std::runtime_error("read: file is closed");
          indigenous->file.clear();
          indigenous->file.seekg(static_cast<std::streamoff>(position),std::ios::beg);
          if(!indigenous->file)throw std::runtime_error("read seek failed");
          indigenous->file.read(reinterpret_cast<char*>(buffer->data()+offset),
            static_cast<std::streamsize>(length));
          if(indigenous->file.bad())throw std::runtime_error("read failed");
          const auto bytes=indigenous->file.gcount();
#else
          const auto bytes=::pread(indigenous->descriptor.load(),buffer->data()+offset,
            length,static_cast<off_t>(position));
          if(bytes<0)fail("pread");
#endif
          return Json::object({{"bytesRead",bytes}});
        });
      };
      handle->close=[host,indigenous]{
        return host->submit<event_loop::Unit>([indigenous]{
#if defined(_WIN32)
          std::lock_guard lock(indigenous->mutex);
          if(!indigenous->closed){indigenous->file.close();indigenous->closed=true;}
#else
          const int descriptor=indigenous->descriptor.exchange(-1);
          if(descriptor>=0&&::close(descriptor)<0)fail("close");
#endif
          return event_loop::Unit{};
        });
      };
      return handle;
    });
  };
  return imports;
}
}
