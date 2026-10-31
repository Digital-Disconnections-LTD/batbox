#include "paste_store/Indigenous.hpp"
#include "runtime/Encoding.hpp"
#include "runtime/IndigenousPath.hpp"
#include "runtime/IndigenousStatTime.hpp"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#ifndef _WIN32
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#else
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <stdexcept>
#include <string>
#include <system_error>

namespace batbox::paste_store {
namespace {
[[noreturn]] void fail(const char* operation){
#ifdef _WIN32
  throw std::system_error(static_cast<int>(::GetLastError()),std::system_category(),operation);
#else
  throw std::system_error(errno,std::generic_category(),operation);
#endif
}
void writeAll(const std::string& path,const std::string& content,int mode){
#ifdef _WIN32
  (void)mode;
  const auto bytes=value::encodeUtf8(content);
  HANDLE file=::CreateFileW(runtime::pathFromUtf8(path).c_str(),GENERIC_WRITE,
    FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,CREATE_ALWAYS,
    FILE_ATTRIBUTE_NORMAL,nullptr);
  if(file==INVALID_HANDLE_VALUE)fail("write paste");
  try{
    for(std::size_t offset=0;offset<bytes.size();){
      const auto wanted=static_cast<DWORD>(std::min<std::size_t>(bytes.size()-offset,0xffffffffu));
      DWORD count=0;
      if(!::WriteFile(file,bytes.data()+offset,wanted,&count,nullptr))fail("write paste");
      if(count==0)throw std::runtime_error("Short paste write");
      offset+=count;
    }
  }catch(...){::CloseHandle(file);throw;}
  if(!::CloseHandle(file))fail("close paste");
#else
  const auto indigenousPath=value::encodeUtf8(path);
  const auto bytes=value::encodeUtf8(content);
  const int descriptor=::open(indigenousPath.c_str(),O_WRONLY|O_CREAT|O_TRUNC,mode);
  if(descriptor<0)fail("write paste");
  try{
    for(std::size_t offset=0;offset<bytes.size();){
      const auto count=::write(descriptor,bytes.data()+offset,bytes.size()-offset);
      if(count<0){if(errno==EINTR)continue;fail("write paste");}
      if(count==0)throw std::runtime_error("Short paste write");
      offset+=static_cast<std::size_t>(count);
    }
  }catch(...){(void)::close(descriptor);throw;}
  if(::close(descriptor)<0)fail("close paste");
#endif
}
std::string readAll(const std::string& path){
#ifdef _WIN32
  std::ifstream input(runtime::pathFromUtf8(path),std::ios::binary);
  if(!input)throw std::system_error(std::make_error_code(
    std::errc::no_such_file_or_directory),"read paste");
  std::string bytes(std::istreambuf_iterator<char>{input},{});
  if(input.bad())throw std::runtime_error("read paste failed");
  return value::decodeUtf8(bytes);
#else
  const auto indigenousPath=value::encodeUtf8(path);
  const int descriptor=::open(indigenousPath.c_str(),O_RDONLY);
  if(descriptor<0)fail("read paste");
  std::string bytes;char buffer[8192];
  try{
    for(;;){
      const auto count=::read(descriptor,buffer,sizeof(buffer));
      if(count<0){if(errno==EINTR)continue;fail("read paste");}
      if(count==0)break;
      bytes.append(buffer,static_cast<std::size_t>(count));
    }
  }catch(...){(void)::close(descriptor);throw;}
  if(::close(descriptor)<0)fail("close paste");
  return value::decodeUtf8(bytes);
#endif
}
std::vector<std::string> list(const std::string& path){
#ifdef _WIN32
  std::vector<std::string> files;
  for(const auto& entry:std::filesystem::directory_iterator(runtime::pathFromUtf8(path)))
    files.push_back(value::decodeUtf8(runtime::pathToUtf8(entry.path().filename())));
  return files;
#else
  DIR* directory=::opendir(value::encodeUtf8(path).c_str());
  if(!directory)fail("readdir paste");
  std::vector<std::string> files;errno=0;
  while(const auto* entry=::readdir(directory)){
    const std::string name=entry->d_name;
    if(name!="."&&name!="..")files.push_back(value::decodeUtf8(name));
    errno=0;
  }
  const int error=errno;
  (void)::closedir(directory);
  if(error){errno=error;fail("readdir paste");}
  return files;
#endif
}
}
Services indigenousServices(event_loop::Loop& loop,
    std::shared_ptr<fs_operations::IndigenousAsyncFsHost> host,
    std::function<std::string()> getConfigHomeDir,
    std::function<void(const std::string&)> logForDebugging){
  if(!host||!getConfigHomeDir||!logForDebugging)
    throw std::invalid_argument("Paste store native imports required");
  Services services;
  services.loop=&loop;
  services.getConfigHomeDir=std::move(getConfigHomeDir);
  services.logForDebugging=std::move(logForDebugging);
  services.join=[](const std::string& left,const std::string& right){
    return runtime::pathToUtf8((runtime::pathFromUtf8(left)/runtime::pathFromUtf8(right))
      .lexically_normal());};
  services.mkdir=[host](const std::string& path,const Json& options){
    return host->submit<event_loop::Unit>([path,options]{
      if(!options.value("recursive",false))
        throw std::invalid_argument("Paste directory requires recursive mkdir");
      std::filesystem::create_directories(runtime::pathFromUtf8(path));
      return event_loop::Unit{};
    });
  };
  services.writeFile=[host](const std::string& path,const std::string& content,
      const Json& options){
    return host->submit<event_loop::Unit>([path,content,options]{
      if(options.value("encoding",std::string{})!="utf8")
        throw std::invalid_argument("Paste write requires utf8 encoding");
      writeAll(path,content,options.value("mode",0600));
      return event_loop::Unit{};
    });
  };
  services.readFile=[host](const std::string& path,const Json& options){
    return host->submit<std::string>([path,options]{
      if(options.value("encoding",std::string{})!="utf8")
        throw std::invalid_argument("Paste read requires utf8 encoding");
      return readAll(path);
    });
  };
  services.readdir=[host](const std::string& path){
    return host->submit<std::vector<std::string>>([path]{return list(path);});};
  services.stat=[host](const std::string& path){
    return host->submit<Json>([path]{
#ifdef _WIN32
      WIN32_FILE_ATTRIBUTE_DATA info{};
      if(!::GetFileAttributesExW(runtime::pathFromUtf8(path).c_str(),
        GetFileExInfoStandard,&info))fail("stat paste");
      ULARGE_INTEGER ticks{};ticks.LowPart=info.ftLastWriteTime.dwLowDateTime;
      ticks.HighPart=info.ftLastWriteTime.dwHighDateTime;
      const double mtime=(static_cast<double>(ticks.QuadPart)-
        116444736000000000.0)/10000.0;
#else
      struct stat info{};
      if(::stat(value::encodeUtf8(path).c_str(),&info)<0)fail("stat paste");
      const double mtime=runtime::statMtimeMillis(info);
#endif
      return Json::object({{"mtimeMs",mtime}});
    });
  };
  services.unlink=[host](const std::string& path){
    return host->submit<event_loop::Unit>([path]{
#ifdef _WIN32
      if(!::DeleteFileW(runtime::pathFromUtf8(path).c_str()))fail("unlink paste");
#else
      if(::unlink(value::encodeUtf8(path).c_str())<0)fail("unlink paste");
#endif
      return event_loop::Unit{};
    });
  };
  services.isENOENT=[](const std::exception_ptr& error){
    try{std::rethrow_exception(error);}
    catch(const std::system_error& value){
      return value.code().default_error_condition()==std::errc::no_such_file_or_directory;}
    catch(...){return false;}
  };
  services.errorToString=[](const std::exception_ptr& error){
    try{std::rethrow_exception(error);}
    catch(const std::exception& value){return std::string("Error: ")+value.what();}
    catch(...){return std::string("Error");}
  };
  return services;
}
}
