#include "shell/TailFile.hpp"
#include "shell/ManagedShellInternal.hpp"
#include "runtime/IndigenousPath.hpp"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#ifndef _WIN32
#include <fcntl.h>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace batbox::shell {
TailResult tailFile(const std::string& path,std::size_t maxBytes){
#ifdef _WIN32
  const auto indigenous=runtime::pathFromUtf8(path);
  std::ifstream stream(indigenous,std::ios::binary|std::ios::ate);
  if(!stream)throw std::runtime_error("tailFile open failed");
  const auto position=stream.tellg();
  if(position<0)throw std::runtime_error("tailFile stat failed");
  const auto size=static_cast<std::size_t>(position);
  if(size==0)return {};
  const auto offset=size>maxBytes?size-maxBytes:0;
  stream.seekg(static_cast<std::streamoff>(offset));
  std::string buffer(size-offset,'\0');
  stream.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));
  buffer.resize(static_cast<std::size_t>(stream.gcount()));
  return {utf8WithReplacement(buffer),buffer.size(),size};
#else
  const int fd=::open(path.c_str(),O_RDONLY|O_CLOEXEC);
  if(fd<0)throw std::runtime_error("tailFile open failed: "+std::string(std::strerror(errno)));
  struct Guard {int fd;~Guard(){::close(fd);}} guard{fd};
  struct stat info{};
  if(::fstat(fd,&info)!=0)throw std::runtime_error("tailFile stat failed: "+std::string(std::strerror(errno)));
  const auto size=static_cast<std::size_t>(info.st_size);
  if(size==0)return {};
  const auto offset=size>maxBytes?size-maxBytes:0;
  const auto bytesToRead=size-offset;
  std::string buffer(bytesToRead,'\0');
  std::size_t totalRead=0;
  while(totalRead<bytesToRead){
    const auto count=::pread(fd,buffer.data()+totalRead,bytesToRead-totalRead,
      static_cast<off_t>(offset+totalRead));
    if(count<0&&errno==EINTR)continue;
    if(count<0)throw std::runtime_error("tailFile read failed: "+std::string(std::strerror(errno)));
    if(count==0)break;
    totalRead+=static_cast<std::size_t>(count);
  }
  buffer.resize(totalRead);
  return {utf8WithReplacement(buffer),totalRead,size};
#endif
}
} // namespace batbox::shell
