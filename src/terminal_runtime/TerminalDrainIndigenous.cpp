#include "terminal_runtime/TerminalControls.hpp"
#include <cerrno>
#include <stdexcept>
#include <system_error>
#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif

namespace batbox::terminal_runtime {
DrainServices indigenousDrainServices(){
  DrainServices result;
#ifdef _WIN32
  result.windows=true;
#else
  result.openNonblockingTty=[](){
    const int fd=::open("/dev/tty",O_RDONLY|O_NONBLOCK);
    if(fd<0)throw std::system_error(errno,std::generic_category());
    return fd;
  };
  result.readTty=[](int fd,char* buffer,std::size_t size)
      ->std::ptrdiff_t{
    const auto count=::read(fd,buffer,size);
    if(count<0)throw std::system_error(errno,std::generic_category());
    return count;
  };
  result.closeTty=[](int fd){
    if(::close(fd)<0)throw std::system_error(errno,std::generic_category());
  };
#endif
  return result;
}
} // namespace batbox::terminal_runtime
