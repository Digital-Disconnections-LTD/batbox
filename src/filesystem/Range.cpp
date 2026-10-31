#include "filesystem/Range.hpp"
#include "filesystem/FileOps.hpp"
#include "runtime/IndigenousPath.hpp"
#include "runtime/IndigenousStatTime.hpp"
#include <cerrno>
#include <stdexcept>
#ifndef _WIN32
#include <sys/stat.h>
#else
#include <filesystem>
#endif
#include <system_error>

namespace batbox::filesystem {
namespace {
constexpr std::size_t fastPathMaxSize = 10 * 1024 * 1024;
[[noreturn]] void tooLarge(std::size_t actual, std::size_t maximum, const RangeServices& services) {
  if (!services.formatFileSize) throw std::invalid_argument("Missing range binding: formatFileSize");
  throw FileTooLargeError(actual,maximum,"File content (" + services.formatFileSize(actual) +
    ") exceeds maximum allowed size (" + services.formatFileSize(maximum) +
    "). Use offset and limit parameters to read specific portions of the file, or search for specific content instead of reading the whole file.");
}
} // namespace

ReadFileRangeResult readFileInRange(const std::string& filePath,
  std::size_t offset, std::optional<std::size_t> maxLines,
  std::optional<std::size_t> maxBytes, std::stop_token stopToken,
  bool truncateOnByteLimit, const RangeServices& services) {
  if (stopToken.stop_requested()) throw std::runtime_error("This operation was aborted");
#ifdef _WIN32
  const auto indigenous=runtime::pathFromUtf8(filePath);
  std::error_code statError;
  const auto status=std::filesystem::status(indigenous,statError);
  if(statError){
    if(statError==std::errc::no_such_file_or_directory)
      throw RangeError(std::errc::no_such_file_or_directory,
        "ENOENT: no such file or directory, stat '"+filePath+"'");
    throw std::system_error(statError,"stat");
  }
  if(std::filesystem::is_directory(status))
    throw std::runtime_error("EISDIR: illegal operation on a directory, read '"+filePath+"'");
  const bool regular=std::filesystem::is_regular_file(status);
  const auto size=regular?std::filesystem::file_size(indigenous,statError):0;
  if(statError)throw std::system_error(statError,"stat");
  if(regular&&size<fastPathMaxSize){
    if(!truncateOnByteLimit&&maxBytes&&size>*maxBytes)tooLarge(size,*maxBytes,services);
    const auto text=readFileBytes(filePath,std::nullopt,stopToken);
    return readFileInRangeFast(text,getFileModificationTimeRaw(filePath),offset,
      maxLines,truncateOnByteLimit?maxBytes:std::nullopt);
  }
#else
  struct stat stats {};
  if (::stat(filePath.c_str(), &stats) != 0) {
    if (errno == ENOENT)
      throw RangeError(std::errc::no_such_file_or_directory,
        "ENOENT: no such file or directory, stat '" + filePath + "'");
    throw std::system_error(errno, std::generic_category(), "stat");
  }
  if (S_ISDIR(stats.st_mode))
    throw std::runtime_error("EISDIR: illegal operation on a directory, read '" + filePath + "'");
  if (S_ISREG(stats.st_mode) && stats.st_size < static_cast<off_t>(fastPathMaxSize)) {
    const std::size_t size = static_cast<std::size_t>(stats.st_size);
    if (!truncateOnByteLimit && maxBytes && size > *maxBytes) tooLarge(size, *maxBytes, services);
    const std::string text = readFileBytes(filePath, std::nullopt, stopToken);
    const double mtime = runtime::statMtimeMillis(stats);
    return readFileInRangeFast(text, mtime, offset, maxLines,
      truncateOnByteLimit ? maxBytes : std::nullopt);
  }
#endif
  return readFileInRangeStreaming(filePath, offset, maxLines, maxBytes,
    truncateOnByteLimit, stopToken, services);
}

} // namespace batbox::filesystem
