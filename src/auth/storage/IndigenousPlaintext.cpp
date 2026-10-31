#include "auth/storage/Storage.hpp"
#include "locking/Async.hpp"
#include "runtime/Encoding.hpp"
#include "runtime/IndigenousPath.hpp"
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <fcntl.h>
#include <stdexcept>
#if !defined(_WIN32)
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <system_error>

namespace batbox::auth::storage {
namespace {
[[noreturn]] void osError(const char* operation){
  throw std::system_error(errno,std::generic_category(),operation);
}
std::string readBytes(const std::string& path){
#if defined(_WIN32)
  std::ifstream input(runtime::pathFromUtf8(value::encodeUtf8(path)),std::ios::binary);
  if(!input)osError("open credential file");
  const std::string output((std::istreambuf_iterator<char>(input)),{});
  if(input.bad())osError("read credential file");
  return output;
#else
  const auto encoded=value::encodeUtf8(path);
  const int fd=::open(encoded.c_str(),O_RDONLY);
  if(fd<0)osError("open credential file");
  std::string output;
  char buffer[8192];
  try{
    for(;;){
      const auto count=::read(fd,buffer,sizeof(buffer));
      if(count<0){if(errno==EINTR)continue;osError("read credential file");}
      if(count==0)break;
      output.append(buffer,static_cast<std::size_t>(count));
    }
  }catch(...){(void)::close(fd);throw;}
  if(::close(fd)!=0)osError("close credential file");
  return output;
#endif
}
void writeBytes(const std::string& path,const std::string& data){
#if defined(_WIN32)
  std::ofstream output(runtime::pathFromUtf8(value::encodeUtf8(path)),
    std::ios::binary|std::ios::trunc);
  if(!output)osError("open credential file for write");
  output.write(data.data(),static_cast<std::streamsize>(data.size()));
  output.close();
  if(!output)osError("write credential file");
#else
  const auto encoded=value::encodeUtf8(path);
  const int fd=::open(encoded.c_str(),O_WRONLY|O_CREAT|O_TRUNC,0666);
  if(fd<0)osError("open credential file for write");
  try{
    std::size_t offset=0;
    while(offset<data.size()){
      const auto count=::write(fd,data.data()+offset,data.size()-offset);
      if(count<0){if(errno==EINTR)continue;osError("write credential file");}
      if(count==0)throw std::runtime_error("Short credential write");
      offset+=static_cast<std::size_t>(count);
    }
  }catch(...){(void)::close(fd);throw;}
  if(::close(fd)!=0)osError("close credential file");
#endif
}
}
PlaintextServices indigenousPlaintextServices(event_loop::Loop& loop,
    std::function<std::string()> configHome){
  PlaintextServices services;
  services.configHome=std::move(configHome);
  services.readFileSync=readBytes;
  services.readFileAsync=[&loop](const std::string& path){
    return locking::runJob<std::string>(loop,[path]{return readBytes(path);});
  };
  services.mkdirSync=[](const std::string& path){
    std::filesystem::create_directories(
      runtime::pathFromUtf8(value::encodeUtf8(path)));
  };
  services.writeFileSync=writeBytes;
  services.chmodSync=[](const std::string& path,int mode){
#if defined(_WIN32)
    std::filesystem::permissions(runtime::pathFromUtf8(value::encodeUtf8(path)),
      static_cast<std::filesystem::perms>(mode&0777),
      std::filesystem::perm_options::replace);
#else
    if(::chmod(value::encodeUtf8(path).c_str(),mode)!=0)
      osError("chmod credential file");
#endif
  };
  services.unlinkSync=[](const std::string& path){
#if defined(_WIN32)
    if(!std::filesystem::remove(runtime::pathFromUtf8(value::encodeUtf8(path))))
      throw std::system_error(std::make_error_code(
        std::errc::no_such_file_or_directory),"delete credential file");
#else
    if(::unlink(value::encodeUtf8(path).c_str())!=0)
      osError("delete credential file");
#endif
  };
  return services;
}
}
