#include "fs_operations/NodeFsIndigenous.hpp"
#include "fs_operations/IndigenousStatPortability.hpp"
#include "diagnostics/SlowScopeBinding.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/IndigenousPath.hpp"
#include "runtime/IndigenousStatTime.hpp"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <system_error>
#if defined(_WIN32)
#include <io.h>
#include <limits>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace batbox::fs_operations {
namespace {
void fail(const char* operation){
  throw std::system_error(errno,std::generic_category(),operation);
}
void requireUtf8(const std::string& value){
  if(value!="utf8"&&value!="utf-8")
    throw std::invalid_argument("Unsupported text encoding: "+value);
}
std::string readBytes(const std::string& path){
  std::ifstream file(runtime::pathFromUtf8(path),std::ios::binary);
  if(!file)fail("readFile");
  const std::string bytes(std::istreambuf_iterator<char>(file),{});
  if(file.bad())fail("readFile");
  return bytes;
}
void writeAll(int fd,const std::string& text){
  std::size_t written=0;
  while(written<text.size()){
#if defined(_WIN32)
    const auto count=::_write(fd,text.data()+written,
      static_cast<unsigned int>(std::min<std::size_t>(
        text.size()-written,std::numeric_limits<unsigned int>::max())));
#else
    const auto count=::write(fd,text.data()+written,text.size()-written);
#endif
    if(count<0){if(errno==EINTR)continue;fail("write");}
    if(count==0)throw std::runtime_error("write returned zero bytes");
    written+=static_cast<std::size_t>(count);
  }
}
std::string readLink(const std::string& path){
#if defined(_WIN32)
  return runtime::pathToUtf8(std::filesystem::read_symlink(
    runtime::pathFromUtf8(path)));
#else
  std::string target(256,'\0');
  for(;;){
    const auto count=::readlink(path.c_str(),target.data(),target.size());
    if(count<0)fail("readlink");
    if(static_cast<std::size_t>(count)<target.size()){
      target.resize(static_cast<std::size_t>(count));return target;
    }
    target.resize(target.size()*2);
  }
#endif
}
Json stats(const std::string& path,bool link){
#if defined(_WIN32)
  return portableStat(path,link);
#else
  struct stat info{};
  if((link?::lstat(path.c_str(),&info): ::stat(path.c_str(),&info))<0)
    fail(link?"lstat":"stat");
  return Json::object({{"size",info.st_size},{"mode",info.st_mode},
    {"mtimeMs",runtime::statMtimeMillis(info)},
    {"isFile",S_ISREG(info.st_mode)!=0},{"isDirectory",S_ISDIR(info.st_mode)!=0},
    {"isSymbolicLink",S_ISLNK(info.st_mode)!=0},
    {"isFIFO",S_ISFIFO(info.st_mode)!=0},{"isSocket",S_ISSOCK(info.st_mode)!=0},
    {"isCharacterDevice",S_ISCHR(info.st_mode)!=0},
    {"isBlockDevice",S_ISBLK(info.st_mode)!=0}});
#endif
}
void recursiveMkdir(const std::string& path,int mode){
#if defined(_WIN32)
  (void)mode; // Indigenous Windows directory ACLs are separate from POSIX modes.
  std::filesystem::create_directories(runtime::pathFromUtf8(path));
#else
  std::filesystem::path current;
  for(const auto& component:std::filesystem::path(path)){
    current/=component;
    const auto name=current.string();
    if(name.empty()||name=="/")continue;
    if(::mkdir(name.c_str(),mode)==0)continue;
    if(errno!=EEXIST)fail("mkdir");
    struct stat info{};
    if(::stat(name.c_str(),&info)<0||!S_ISDIR(info.st_mode))fail("mkdir");
  }
#endif
}
std::vector<std::string> listNames(const std::string& path){
#if defined(_WIN32)
  std::vector<std::string> names;
  for(const auto& entry:std::filesystem::directory_iterator(
      runtime::pathFromUtf8(path)))
    names.push_back(runtime::pathToUtf8(entry.path().filename()));
  return names;
#else
  DIR* directory=::opendir(path.c_str());
  if(!directory)fail("readdir");
  std::vector<std::string> names;
  errno=0;
  while(const auto* entry=::readdir(directory)){
    if(std::strcmp(entry->d_name,".")&&std::strcmp(entry->d_name,".."))
      names.emplace_back(entry->d_name);
    errno=0;
  }
  const auto readError=errno;
  ::closedir(directory);
  if(readError){errno=readError;fail("readdir");}
  return names;
#endif
}
}

NodeFsSyncImports indigenousNodeFsSyncImports(
    std::shared_ptr<diagnostics::SlowOperations> slowOperations){
  NodeFsSyncImports imports;
  imports.beginSlowLogging=diagnostics::bindSlowLogging(
    std::move(slowOperations));
  imports.cwd=[](){return runtime::pathToUtf8(std::filesystem::current_path());};
  imports.existsSync=[](const std::string& path){
    std::error_code error;return std::filesystem::exists(
      runtime::pathFromUtf8(path),error);};
  imports.statSync=[](const std::string& path){return stats(path,false);};
  imports.lstatSync=[](const std::string& path){return stats(path,true);};
  imports.readFileSync=[](const std::string& path,const std::string& encoding){
    requireUtf8(encoding);return value::decodeUtf8(readBytes(path));};
  imports.readFileBytesSync=[](const std::string& path){
    const auto bytes=readBytes(path);
    return std::vector<unsigned char>(bytes.begin(),bytes.end());};
  imports.openSync=[](const std::string& path,const std::string& flag,
      const std::optional<Json>& mode){
#if defined(_WIN32)
    const int flags=flag=="r"?_O_RDONLY|_O_BINARY:flag=="ax"?
      _O_WRONLY|_O_APPEND|_O_CREAT|_O_EXCL|_O_BINARY:-1;
#else
    const int flags=flag=="r"?O_RDONLY:flag=="ax"?
      O_WRONLY|O_APPEND|O_CREAT|O_EXCL:-1;
#endif
    if(flags<0)throw std::invalid_argument("Unsupported open flag: "+flag);
    const int permissions=mode&&mode->is_number_integer()?mode->get<int>():0666;
#if defined(_WIN32)
    const int fd=::_wopen(runtime::pathFromUtf8(path).c_str(),flags,permissions);
#else
    const int fd=::open(path.c_str(),flags,permissions);
#endif
    if(fd<0)fail("open");
    return fd;
  };
  imports.readSync=[](int fd,std::vector<unsigned char>& buffer,
      std::size_t offset,std::size_t length,std::size_t position){
#if defined(_WIN32)
    const auto old=::_lseeki64(fd,0,SEEK_CUR);
    if(old<0||::_lseeki64(fd,static_cast<__int64>(position),SEEK_SET)<0)
      fail("read seek");
    const auto count=::_read(fd,buffer.data()+offset,
      static_cast<unsigned int>(std::min<std::size_t>(length,
        std::numeric_limits<unsigned int>::max())));
    const int readError=errno;
    if(::_lseeki64(fd,old,SEEK_SET)<0)fail("read restore");
    if(count<0){errno=readError;fail("read");}
#else
    const auto count=::pread(fd,buffer.data()+offset,length,
      static_cast<off_t>(position));
    if(count<0)fail("read");
#endif
    return static_cast<std::size_t>(count);
  };
  imports.closeSync=[](int fd){
#if defined(_WIN32)
    if(::_close(fd)<0)fail("close");
#else
    if(::close(fd)<0)fail("close");
#endif
  };
  imports.appendFdSync=[](int fd,const std::string& text){writeAll(fd,text);};
  imports.appendPathSync=[](const std::string& path,const std::string& text){
#if defined(_WIN32)
    const int fd=::_wopen(runtime::pathFromUtf8(path).c_str(),
      _O_WRONLY|_O_APPEND|_O_CREAT|_O_BINARY,0666);
#else
    const int fd=::open(path.c_str(),O_WRONLY|O_APPEND|O_CREAT,0666);
#endif
    if(fd<0)fail("open");
    try{writeAll(fd,text);}catch(...){const auto error=std::current_exception();
#if defined(_WIN32)
      ::_close(fd);std::rethrow_exception(error);}
    if(::_close(fd)<0)fail("close");
#else
      ::close(fd);std::rethrow_exception(error);}
    if(::close(fd)<0)fail("close");
#endif
  };
  imports.errnoCode=[](const std::exception_ptr& error)->std::optional<std::string>{
    try{std::rethrow_exception(error);}
    catch(const std::system_error& value){
      if(value.code()==std::errc::file_exists)return "EEXIST";
      if(value.code()==std::errc::no_such_file_or_directory)return "ENOENT";
      if(value.code()==std::errc::permission_denied)return "EACCES";
      return std::nullopt;
    }catch(...){return std::nullopt;}
  };
  imports.copyFileSync=[](const std::string& a,const std::string& b){
    std::filesystem::copy_file(runtime::pathFromUtf8(a),runtime::pathFromUtf8(b),
      std::filesystem::copy_options::overwrite_existing);};
  imports.unlinkSync=[](const std::string& path){
#if defined(_WIN32)
    if(!std::filesystem::remove(runtime::pathFromUtf8(path)))
      throw std::system_error(std::make_error_code(
        std::errc::no_such_file_or_directory),"unlink");
#else
    if(::unlink(path.c_str())<0)fail("unlink");
#endif
  };
  imports.renameSync=[](const std::string& a,const std::string& b){
#if defined(_WIN32)
    std::filesystem::rename(runtime::pathFromUtf8(a),runtime::pathFromUtf8(b));
#else
    if(::rename(a.c_str(),b.c_str())<0)fail("rename");
#endif
  };
  imports.linkSync=[](const std::string& a,const std::string& b){
#if defined(_WIN32)
    std::filesystem::create_hard_link(runtime::pathFromUtf8(a),
      runtime::pathFromUtf8(b));
#else
    if(::link(a.c_str(),b.c_str())<0)fail("link");
#endif
  };
  imports.symlinkSync=[](const std::string& a,const std::string& b,
      const std::optional<std::string>& type){
#if defined(_WIN32)
    if(type&&*type=="dir")std::filesystem::create_directory_symlink(
      runtime::pathFromUtf8(a),runtime::pathFromUtf8(b));
    else std::filesystem::create_symlink(runtime::pathFromUtf8(a),
      runtime::pathFromUtf8(b));
#else
    (void)type;
    if(::symlink(a.c_str(),b.c_str())<0)fail("symlink");
#endif
  };
  imports.readlinkSync=readLink;
  imports.realpathSync=[](const std::string& path){
    return runtime::pathToUtf8(std::filesystem::canonical(
      runtime::pathFromUtf8(path)));};
  imports.mkdirSync=[](const std::string& path,const Json& options){
    const int mode=options.contains("mode")&&options["mode"].is_number_integer()?
      options["mode"].get<int>():0777;
    recursiveMkdir(path,mode);
  };
  imports.readdirSync=[](const std::string& path,bool){
    Json result=Json::array();
    for(const auto& name:listNames(path)){
      const auto full=runtime::pathToUtf8(runtime::pathFromUtf8(path)/
        runtime::pathFromUtf8(name));
      const auto info=stats(full,true);
      result.push_back(Json::object({{"name",name},
        {"isFile",info["isFile"]},{"isDirectory",info["isDirectory"]},
        {"isSymbolicLink",info["isSymbolicLink"]}}));
    }
    return result;
  };
  imports.readdirStringSync=listNames;
  imports.rmdirSync=[](const std::string& path){
#if defined(_WIN32)
    if(!std::filesystem::remove(runtime::pathFromUtf8(path)))
      throw std::system_error(std::make_error_code(
        std::errc::no_such_file_or_directory),"rmdir");
#else
    if(::rmdir(path.c_str())<0)fail("rmdir");
#endif
  };
  imports.rmSync=[](const std::string& path,const std::optional<Json>& options){
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
  };
  return imports;
}
}
