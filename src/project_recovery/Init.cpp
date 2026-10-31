#include "project_recovery/Internal.hpp"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <system_error>
#ifdef _WIN32
#include <io.h>
#include <sys/stat.h>
#else
#include <unistd.h>
#endif

namespace batbox::project_recovery {
namespace fs=std::filesystem;
namespace {
bool ensureDirectory(const fs::path& path,InitResult& result){
  std::error_code error;
  if(detail::plainDirectory(path))return true;
  if(fs::create_directory(path,error))return true;
  if(!error&&detail::plainDirectory(path))return true;
  result.warnings.push_back("Could not create directory "+path.generic_string()+
    (error?": "+error.message():": path already exists"));
  return false;
}
void createMissing(const fs::path& path,const char* content,InitResult& result){
#ifdef _WIN32
  const int descriptor=_wopen(path.c_str(),_O_WRONLY|_O_CREAT|_O_EXCL|_O_BINARY,
    _S_IREAD|_S_IWRITE);
#else
  const int descriptor=::open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL,0666);
#endif
  if(descriptor<0){
    if(errno==EEXIST){
      result.existing.push_back(path);
      if(!detail::plainFile(path))
        result.warnings.push_back("Existing recovery path is not a regular file: "+path.generic_string());
    }
    else result.warnings.push_back("Could not create "+path.generic_string()+": "+std::strerror(errno));
    return;
  }
#ifdef _WIN32
  std::FILE* file=_fdopen(descriptor,"wb");
#else
  std::FILE* file=::fdopen(descriptor,"wb");
#endif
  if(!file){
#ifdef _WIN32
    _close(descriptor);
#else
    ::close(descriptor);
#endif
    result.warnings.push_back("Could not open new recovery file "+path.generic_string());
    return;
  }
  const auto length=std::strlen(content);
  const bool written=std::fwrite(content,1,length,file)==length;
  const bool closed=std::fclose(file)==0;
  result.created.push_back(path);
  if(!written||!closed)result.warnings.push_back("Could not finish writing "+path.generic_string());
}
}
InitResult init(const fs::path& cwd){
  InitResult result;
  const auto existing=findDirectory(cwd);
  result.projectRoot=existing?existing->parent_path():detail::projectRoot(cwd);
  if(result.projectRoot.empty()){
    result.warnings.push_back("Recovery initialization needs an existing working directory");
    return result;
  }
  result.directory=result.projectRoot/".batbox";
  if(!ensureDirectory(result.directory,result))return result;
  for(const auto& entry:detail::defaults())createMissing(result.directory/entry.name,entry.content,result);
  return result;
}
}
