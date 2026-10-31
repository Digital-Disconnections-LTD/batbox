#include "media/PDFFile.hpp"
#include "runtime/IndigenousPath.hpp"
#include <cctype>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <system_error>
#else
#include <sys/stat.h>
#endif

namespace batbox::media::indigenous {
namespace {
const char* errorCode(int code){
  switch(code){
    case ENOENT:return "ENOENT";
    case EACCES:return "EACCES";
    case EPERM:return "EPERM";
    case ENOTDIR:return "ENOTDIR";
    case ELOOP:return "ELOOP";
    case ENAMETOOLONG:return "ENAMETOOLONG";
    default:return "EUNKNOWN";
  }
}
}
std::size_t statSize(const std::string& filePath){
#ifdef _WIN32
  WIN32_FILE_ATTRIBUTE_DATA info{};
  if(::GetFileAttributesExW(runtime::pathFromUtf8(filePath).c_str(),
      GetFileExInfoStandard,&info))
    return (static_cast<std::size_t>(info.nFileSizeHigh)<<32)|info.nFileSizeLow;
  const int code=std::error_code(static_cast<int>(::GetLastError()),
    std::system_category()).default_error_condition().value();
  std::string message=std::generic_category().message(code);
#else
  struct stat info{};
  if(::stat(filePath.c_str(),&info)==0)return static_cast<std::size_t>(info.st_size);
  const int code=errno;
  std::string message=std::strerror(code);
#endif
  if(!message.empty())message[0]=static_cast<char>(std::tolower(static_cast<unsigned char>(message[0])));
  throw std::runtime_error(std::string(errorCode(code))+": "+message+", stat '"+filePath+"'");
}
} // namespace batbox::media::indigenous
