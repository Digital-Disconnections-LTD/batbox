#include "fs_operations/NodeFsAsyncIndigenous.hpp"
#include "fs_operations/IndigenousStatPortability.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/IndigenousPath.hpp"
#include "runtime/IndigenousStatTime.hpp"
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <system_error>
#if !defined(_WIN32)
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace batbox::fs_operations {
namespace {
void fail(const char* operation){
  throw std::system_error(errno,std::generic_category(),operation);
}
void makeDirectories(const std::string& path,int mode){
#if defined(_WIN32)
  (void)mode; // Windows ACLs are not POSIX mode bits.
  std::filesystem::create_directories(runtime::pathFromUtf8(path));
#else
  std::filesystem::path current;
  for(const auto& component:std::filesystem::path(path)){
    current/=component;
    const auto value=current.string();
    if(value.empty()||value=="/")continue;
    if(::mkdir(value.c_str(),mode)==0)continue;
    if(errno!=EEXIST)fail("mkdir");
    struct stat info{};
    if(::stat(value.c_str(),&info)<0||!S_ISDIR(info.st_mode))fail("mkdir");
  }
#endif
}
void removePath(const std::string& path,const std::optional<Json>& options){
  const bool recursive=options&&options->is_object()&&
    options->value("recursive",false);
  const bool force=options&&options->is_object()&&options->value("force",false);
  std::error_code error;
  const auto indigenous=runtime::pathFromUtf8(path);
  const bool exists=std::filesystem::exists(indigenous,error)||
    std::filesystem::is_symlink(indigenous,error);
  if(!exists){if(force)return;errno=ENOENT;fail("rm");}
  if(recursive)std::filesystem::remove_all(indigenous);
  else{
    if(std::filesystem::is_directory(indigenous)){errno=EISDIR;fail("rm");}
    std::filesystem::remove(indigenous);
  }
}
Json listDirectory(const std::string& path){
#if defined(_WIN32)
  Json names=Json::array();
  for(const auto& entry:std::filesystem::directory_iterator(
      runtime::pathFromUtf8(path))){
    const auto status=entry.symlink_status();
    names.push_back(Json::object({{"name",runtime::pathToUtf8(
      entry.path().filename())},
      {"isFile",status.type()==std::filesystem::file_type::regular},
      {"isDirectory",status.type()==std::filesystem::file_type::directory},
      {"isSymbolicLink",status.type()==std::filesystem::file_type::symlink}}));
  }
  return names;
#else
  DIR* directory=::opendir(path.c_str());
  if(!directory)fail("readdir");
  Json names=Json::array();
  errno=0;
  while(const auto* entry=::readdir(directory)){
    if(std::strcmp(entry->d_name,".")&&std::strcmp(entry->d_name,"..")){
      const auto full=(std::filesystem::path(path)/entry->d_name).string();
      struct stat info{};
      if(::lstat(full.c_str(),&info)<0){const auto error=errno;::closedir(directory);
        errno=error;fail("lstat");}
      names.push_back(Json::object({{"name",entry->d_name},
        {"isFile",S_ISREG(info.st_mode)!=0},
        {"isDirectory",S_ISDIR(info.st_mode)!=0},
        {"isSymbolicLink",S_ISLNK(info.st_mode)!=0}}));
    }
    errno=0;
  }
  const auto readError=errno;
  ::closedir(directory);
  if(readError){errno=readError;fail("readdir");}
  return names;
#endif
}
}

IndigenousAsyncFsHost::IndigenousAsyncFsHost(event_loop::Loop& loop,unsigned workers):loop_(loop){
  if(workers==0)throw std::invalid_argument("At least one file worker is required");
  workers_.reserve(workers);
  for(unsigned i=0;i<workers;++i)workers_.emplace_back([this]{work();});
}
IndigenousAsyncFsHost::~IndigenousAsyncFsHost(){
  {std::lock_guard lock(mutex_);stopping_=true;}
  ready_.notify_all();
  for(auto& worker:workers_)worker.join();
}
void IndigenousAsyncFsHost::enqueue(std::function<void()> operation){
  {std::lock_guard lock(mutex_);
    if(stopping_)throw std::logic_error("Filesystem workers stopped");
    queue_.push_back(runtime::bindAsyncContext(std::move(operation)));}
  ready_.notify_one();
}
void IndigenousAsyncFsHost::work(){
  for(;;){
    std::function<void()> operation;
    {std::unique_lock lock(mutex_);
      ready_.wait(lock,[this]{return stopping_||!queue_.empty();});
      if(queue_.empty())return;
      operation=std::move(queue_.front());queue_.pop_front();}
    operation();
  }
}

NodeFsAsyncImports indigenousNodeFsAsyncImports(event_loop::Loop& loop,
    std::shared_ptr<IndigenousAsyncFsHost> host){
  if(!host)throw std::invalid_argument("Filesystem worker host is required");
  NodeFsAsyncImports imports;
  imports.loop=&loop;
  imports.stat=[host](const std::string& path){
    return host->submit<Json>([path]{
#if defined(_WIN32)
      const auto info=portableStat(path,false);
      return Json::object({{"size",info["size"]},{"mode",info["mode"]},
        {"mtimeMs",info["mtimeMs"]}});
#else
      struct stat info{};if(::stat(path.c_str(),&info)<0)fail("stat");
      return Json::object({{"size",info.st_size},{"mode",info.st_mode},
        {"mtimeMs",runtime::statMtimeMillis(info)}});
#endif
    });
  };
  imports.readdir=[host](const std::string& path,bool){
    return host->submit<Json>([path]{return listDirectory(path);});};
  imports.unlink=[host](const std::string& path){
    return host->submit<event_loop::Unit>([path]{
#if defined(_WIN32)
      if(!std::filesystem::remove(runtime::pathFromUtf8(path)))
        throw std::system_error(std::make_error_code(
          std::errc::no_such_file_or_directory),"unlink");
#else
      if(::unlink(path.c_str())<0)fail("unlink");
#endif
      return event_loop::Unit{};});};
  imports.rmdir=[host](const std::string& path){
    return host->submit<event_loop::Unit>([path]{
#if defined(_WIN32)
      if(!std::filesystem::remove(runtime::pathFromUtf8(path)))
        throw std::system_error(std::make_error_code(
          std::errc::no_such_file_or_directory),"rmdir");
#else
      if(::rmdir(path.c_str())<0)fail("rmdir");
#endif
      return event_loop::Unit{};});};
  imports.rm=[host](const std::string& path,const std::optional<Json>& options){
    return host->submit<event_loop::Unit>([path,options]{
      removePath(path,options);return event_loop::Unit{};});};
  imports.mkdir=[host](const std::string& path,const Json& options){
    return host->submit<event_loop::Unit>([path,options]{
      const int mode=options.contains("mode")&&options["mode"].is_number_integer()?
        options["mode"].get<int>():0777;
      makeDirectories(path,mode);return event_loop::Unit{};});};
  imports.readFile=[host](const std::string& path,const std::string& encoding){
    return host->submit<std::string>([path,encoding]{
      if(encoding!="utf8"&&encoding!="utf-8")
        throw std::invalid_argument("Unsupported text encoding: "+encoding);
      std::ifstream file(runtime::pathFromUtf8(path),std::ios::binary);
      if(!file)fail("readFile");
      const std::string bytes(std::istreambuf_iterator<char>(file),{});
      if(file.bad())fail("readFile");
      return value::decodeUtf8(bytes);
    });
  };
  imports.rename=[host](const std::string& a,const std::string& b){
    return host->submit<event_loop::Unit>([a,b]{
#if defined(_WIN32)
      std::filesystem::rename(runtime::pathFromUtf8(a),runtime::pathFromUtf8(b));
#else
      if(::rename(a.c_str(),b.c_str())<0)fail("rename");
#endif
      return event_loop::Unit{};});};
  imports.errnoCode=[](const std::exception_ptr& error)->std::optional<std::string>{
    try{std::rethrow_exception(error);}
    catch(const std::system_error& value){
      if(value.code()==std::errc::file_exists)return "EEXIST";
      return std::nullopt;
    }catch(...){return std::nullopt;}
  };
  return imports;
}
}
