#include "auth/Credential.hpp"
#include "runtime/Encoding.hpp"
#include "runtime/IndigenousPath.hpp"
#include <cerrno>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <system_error>
#if !defined(_WIN32)
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace batbox::auth {
namespace {
[[noreturn]] void osError(const char* action){
  throw std::system_error(errno,std::generic_category(),action);
}
std::string readText(const std::string& path){
#if defined(_WIN32)
  std::ifstream input(runtime::pathFromUtf8(value::encodeUtf8(path)),std::ios::binary);
  if(!input)osError("read credential");
  const std::string bytes((std::istreambuf_iterator<char>(input)),{});
  if(input.bad())osError("read credential");
  return value::decodeUtf8(bytes);
#else
  const auto encoded=value::encodeUtf8(path);
  const int fd=::open(encoded.c_str(),O_RDONLY);
  if(fd<0)osError("read credential");
  std::string result;
  try{
    char bytes[4096];
    for(;;){
      const auto count=::read(fd,bytes,sizeof(bytes));
      if(count<0){if(errno==EINTR)continue;osError("read credential");}
      if(count==0)break;
      result.append(bytes,static_cast<std::size_t>(count));
    }
  }catch(...){::close(fd);throw;}
  if(::close(fd)!=0)osError("close credential");
  return value::decodeUtf8(result);
#endif
}
void writeText(const std::string& path,const std::string& text,int mode){
#if defined(_WIN32)
  const auto indigenous=runtime::pathFromUtf8(value::encodeUtf8(path));
  const auto bytes=value::encodeUtf8(text);
  std::ofstream output(indigenous,std::ios::binary|std::ios::trunc);
  if(!output)osError("write credential");
  output.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));
  output.close();if(!output)osError("write credential");
  std::filesystem::permissions(indigenous,
    static_cast<std::filesystem::perms>(mode&0777),
    std::filesystem::perm_options::replace);
#else
  const auto encoded=value::encodeUtf8(path);
  const auto bytes=value::encodeUtf8(text);
  const int fd=::open(encoded.c_str(),O_WRONLY|O_CREAT|O_TRUNC,mode);
  if(fd<0)osError("write credential");
  try{
    std::size_t offset=0;
    while(offset<bytes.size()){
      const auto count=::write(fd,bytes.data()+offset,bytes.size()-offset);
      if(count<0){if(errno==EINTR)continue;osError("write credential");}
      if(count==0)throw std::runtime_error("Short credential write");
      offset+=static_cast<std::size_t>(count);
    }
  }catch(...){::close(fd);throw;}
  if(::close(fd)!=0)osError("close credential");
#endif
}
}
CredentialServices indigenousCredentialServices(CredentialPaths paths,
    std::function<std::optional<std::string>(const std::string&)> environment,
    std::function<bool()> remoteMode,
    std::function<void(const std::string&,const std::string&)> debug){
  CredentialServices services;
  services.paths=std::move(paths);
  services.environment=std::move(environment);
#if defined(__APPLE__)
  services.platform=[](){return "darwin";};
#elif defined(__FreeBSD__)
  services.platform=[](){return "freebsd";};
#elif defined(_WIN32)
  services.platform=[](){return "win32";};
#else
  services.platform=[](){return "linux";};
#endif
  services.remoteMode=std::move(remoteMode);
  services.readFileSync=readText;
  services.mkdirRecursive=[](const std::string& path,int mode){
#if defined(_WIN32)
    (void)mode; // Windows directory ACLs require a separate owner policy.
    std::filesystem::create_directories(runtime::pathFromUtf8(value::encodeUtf8(path)));
#else
    std::filesystem::path current;
    for(const auto& part:std::filesystem::path(value::encodeUtf8(path))){
      current/=part;
      if(::mkdir(current.c_str(),static_cast<mode_t>(mode))==0)continue;
      if(errno!=EEXIST)osError("mkdir credential directory");
      struct stat status{};
      if(::stat(current.c_str(),&status)!=0)
        osError("mkdir credential directory");
      if(!S_ISDIR(status.st_mode))
        throw std::system_error(ENOTDIR,std::generic_category(),
          "mkdir credential directory");
    }
#endif
  };
  services.writeFileSync=writeText;
  services.debug=std::move(debug);
  return services;
}
}
