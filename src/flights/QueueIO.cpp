#include "flights/WriteQueue.hpp"
#include "flights/IndigenousPath.hpp"
#include <cerrno>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <system_error>
#if !defined(_WIN32)
#include <sys/stat.h>
#include <unistd.h>
#endif
namespace batbox::flights {
QueueServices indigenousQueueServices() {
  QueueServices services;
  services.appendFile=[](const std::string& file,const std::string& content){
#if defined(_WIN32)
    std::ofstream output(indigenousPath(file),std::ios::binary|std::ios::app);
    if(!output)throw std::system_error(errno,std::generic_category(),"appendFile");
    output.write(content.data(),static_cast<std::streamsize>(content.size()));
    output.close();
    if(!output)throw std::system_error(errno?errno:EIO,std::generic_category(),"appendFile");
#else
    const int fd=::open(file.c_str(),O_WRONLY|O_APPEND|O_CREAT|O_CLOEXEC,0600);
    if(fd<0)throw std::system_error(errno,std::generic_category(),"appendFile");
    std::size_t written=0;
    while(written<content.size()) {
      const auto count=::write(fd,content.data()+written,content.size()-written);
      if(count<0 && errno==EINTR)continue;
      if(count<=0){const auto error=count<0?errno:EIO;::close(fd);throw std::system_error(error,std::generic_category(),"appendFile");}
      written+=static_cast<std::size_t>(count);
    }
    if(::close(fd)<0)throw std::system_error(errno,std::generic_category(),"appendFile close");
#endif
  };
  services.mkdir=[](const std::string& directory){
#if defined(_WIN32)
    std::filesystem::create_directories(indigenousPath(directory));
#else
    // mkdir({recursive:true,mode:0700}) applies mode to newly created parents.
    std::filesystem::path current;
    for(const auto& part:std::filesystem::path(directory)) {
      current/=part;
      if(::mkdir(current.c_str(),0700)<0) {
        const auto error=errno;struct stat info{};
        if(error!=EEXIST || ::stat(current.c_str(),&info)<0 || !S_ISDIR(info.st_mode))
          throw std::system_error(error,std::generic_category(),"mkdir");
      }
    }
#endif
  };
  return services;
}
}
