#include "bats/IndigenousMissionOutput.hpp"
#include "runtime/IndigenousPath.hpp"
#include <cerrno>
#include <system_error>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace batbox::bats {
std::string initializeMissionOutputAsSymlink(const std::filesystem::path& output,
    const std::filesystem::path& target,
    const std::function<void(const std::exception&)>& logError){
  try {
    std::filesystem::create_directories(output.parent_path());
    try {
      std::filesystem::create_symlink(target,output);
    } catch (...) {
      if(!std::filesystem::remove(output))
        throw std::filesystem::filesystem_error("unlink",output,
          std::make_error_code(std::errc::no_such_file_or_directory));
      std::filesystem::create_symlink(target,output);
    }
    return runtime::pathToUtf8(output);
  } catch (const std::exception& error) {
    if(logError)logError(error);
  }
  std::filesystem::create_directories(output.parent_path());
#ifdef _WIN32
  const int fd=_wopen(output.c_str(),_O_WRONLY|_O_CREAT|_O_EXCL|_O_BINARY,
    _S_IREAD|_S_IWRITE);
#else
  const int fd=::open(output.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0666);
#endif
  if(fd<0)throw std::filesystem::filesystem_error("open",output,
    std::error_code(errno,std::generic_category()));
#ifdef _WIN32
  const int closed=_close(fd);
#else
  const int closed=::close(fd);
#endif
  if(closed<0)throw std::filesystem::filesystem_error("close",output,
    std::error_code(errno,std::generic_category()));
  return runtime::pathToUtf8(output);
}
}
