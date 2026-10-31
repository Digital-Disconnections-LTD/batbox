#include "runtime/AsyncLaunch.hpp"
#include "instructions/project/async/AsyncDiscovery.hpp"
#include "runtime/Encoding.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/IndigenousPath.hpp"
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <iterator>
#ifndef _WIN32
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <memory>
#include <stdexcept>
#include <system_error>
#include <thread>

namespace batbox::instructions::project::async {
namespace {
template<class T,class F>Deferred<T> worker(event_loop::Loop& loop,F work){
  Deferred<T> output(loop);
  auto keep=std::make_shared<event_loop::Loop::KeepAlive>(loop.hold());
  const auto dispatcher=loop.dispatcher();
  try{
    ::batbox::runtime::contextThread([output,keep,dispatcher,work=std::move(work)]()mutable{
      try{
        auto value=work();
        dispatcher.postMission([output,keep,value=std::move(value)]()mutable{
          output.resolve(std::move(value));keep->release();});
      }catch(...){const auto error=std::current_exception();
        try{dispatcher.postMission([output,keep,error]{
          output.reject(error);keep->release();});}catch(...){}
      }
    }).detach();
  }catch(...){output.reject(std::current_exception());keep->release();}
  return output;
}
std::string readFile(const std::string& path){
#ifdef _WIN32
  const auto indigenous=runtime::pathFromUtf8(path);
  if(std::filesystem::is_directory(indigenous))
    throw std::system_error(std::make_error_code(std::errc::is_a_directory),"readFile");
  std::ifstream stream(indigenous,std::ios::binary);
  if(!stream)throw std::system_error(std::make_error_code(
    std::errc::no_such_file_or_directory),"readFile");
  std::string bytes(std::istreambuf_iterator<char>{stream},{});
  if(stream.bad())throw std::runtime_error("readFile failed");
  return value::decodeUtf8(bytes,false);
#else
  const auto encoded=value::encodeUtf8(path);
  const int fd=::open(encoded.c_str(),O_RDONLY|O_CLOEXEC);
  if(fd<0)throw std::system_error(errno,std::generic_category(),"readFile");
  try{
    struct stat metadata{};
    if(::fstat(fd,&metadata)<0)throw std::system_error(errno,std::generic_category(),"readFile");
    if(S_ISDIR(metadata.st_mode))throw std::system_error(EISDIR,std::generic_category(),"readFile");
    std::string bytes;char buffer[65536];
    for(;;){const auto count=::read(fd,buffer,sizeof(buffer));
      if(count<0){if(errno==EINTR)continue;
        throw std::system_error(errno,std::generic_category(),"readFile");}
      if(count==0)break;
      bytes.append(buffer,static_cast<std::size_t>(count));
    }
    ::close(fd);return value::decodeUtf8(bytes,false);
  }catch(...){::close(fd);throw;}
#endif
}
std::vector<Dirent> readdir(const std::string& path){
#ifdef _WIN32
  std::vector<Dirent> entries;
  for(const auto& entry:std::filesystem::directory_iterator(runtime::pathFromUtf8(path))){
    const auto status=entry.status();
    entries.push_back({value::decodeUtf8(runtime::pathToUtf8(entry.path().filename()),false),
      std::filesystem::is_directory(status),std::filesystem::is_regular_file(status)});
  }
  return entries;
#else
  const auto encoded=value::encodeUtf8(path);
  DIR* dir=::opendir(encoded.c_str());
  if(!dir)throw std::system_error(errno,std::generic_category(),"readdir");
  try{
    std::vector<Dirent> entries;
    for(;;){errno=0;auto* entry=::readdir(dir);
      if(!entry){if(errno)throw std::system_error(errno,std::generic_category(),"readdir");break;}
      const std::string name=value::decodeUtf8(entry->d_name,false);
      if(name=="."||name=="..")continue;
      entries.push_back({name,entry->d_type==DT_DIR,entry->d_type==DT_REG});
    }
    ::closedir(dir);return entries;
  }catch(...){::closedir(dir);throw;}
#endif
}
FileKind statPath(const std::string& path){
#ifdef _WIN32
  const auto status=std::filesystem::status(runtime::pathFromUtf8(path));
  return {std::filesystem::is_directory(status),std::filesystem::is_regular_file(status)};
#else
  const auto encoded=value::encodeUtf8(path);
  struct stat result{};
  if(::stat(encoded.c_str(),&result)<0)
    throw std::system_error(errno,std::generic_category(),"stat");
  return {static_cast<bool>(S_ISDIR(result.st_mode)),
    static_cast<bool>(S_ISREG(result.st_mode))};
#endif
}
}
Services indigenousAsyncFs(event_loop::Loop& loop,DiscoveryServices sync){
  Services services;services.sync=std::move(sync);
  services.readFile=[&loop](const std::string& path){
    return worker<std::string>(loop,[path]{return readFile(path);});};
  services.readdir=[&loop](const std::string& path){
    return worker<std::vector<Dirent>>(loop,[path]{return readdir(path);});};
  services.stat=[&loop](const std::string& path){
    return worker<FileKind>(loop,[path]{return statPath(path);});};
  return services;
}
} // namespace batbox::instructions::project::async
