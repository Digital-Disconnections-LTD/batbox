#include "filesystem/Range.hpp"
#include "filesystem/FileOps.hpp"
#include "runtime/IndigenousPath.hpp"
#include "runtime/IndigenousStatTime.hpp"
#include <cerrno>
#include <fstream>
#include <limits>
#include <stdexcept>
#ifndef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <system_error>
#include <vector>

namespace batbox::filesystem {
namespace {
[[noreturn]] void tooLarge(std::size_t actual, std::size_t maximum, const RangeServices& services) {
  if (!services.formatFileSize) throw std::invalid_argument("Missing range binding: formatFileSize");
  throw FileTooLargeError(actual,maximum,"File content (" + services.formatFileSize(actual) +
    ") exceeds maximum allowed size (" + services.formatFileSize(maximum) +
    "). Use offset and limit parameters to read specific portions of the file, or search for specific content instead of reading the whole file.");
}
std::string join(const std::vector<std::string>& lines) {
  std::string content;
  bool first = true;
  for (const auto& line : lines) {
    if (!first) content += '\n';
    content += line; first = false;
  }
  return content;
}
} // namespace

ReadFileRangeResult readFileInRangeStreaming(const std::string& filePath,
  std::size_t offset, std::optional<std::size_t> maxLines,
  std::optional<std::size_t> maxBytes, bool truncateOnByteLimit,
  std::stop_token stopToken, const RangeServices& services) {
#ifdef _WIN32
  std::ifstream stream(runtime::pathFromUtf8(filePath),std::ios::binary);
  if(!stream)throw std::system_error(errno,std::generic_category(),"open");
#else
  const int fd = ::open(filePath.c_str(), O_RDONLY);
  if (fd < 0) throw std::system_error(errno, std::generic_category(), "open");
#endif
  try {
#ifdef _WIN32
    const double mtime=getFileModificationTimeRaw(filePath);
#else
    struct stat stats {};
    const double mtime = ::fstat(fd, &stats) == 0
      ? runtime::statMtimeMillis(stats) : 0;
#endif
    std::size_t endLine = maxLines && *maxLines <= std::numeric_limits<std::size_t>::max() - offset
      ? offset + *maxLines : std::numeric_limits<std::size_t>::max();
    std::size_t totalBytesRead = 0, selectedBytes = 0, lineIndex = 0;
    bool truncated = false, firstChunk = true;
    std::vector<std::string> selected;
    std::string partial;
    std::string chunk(512 * 1024, '\0');
    for (;;) {
      if (stopToken.stop_requested()) throw std::runtime_error("This operation was aborted");
#ifdef _WIN32
      stream.read(chunk.data(),static_cast<std::streamsize>(chunk.size()));
      const auto count=stream.gcount();
      if(count==0&&stream.bad())throw std::runtime_error("read failed");
#else
      const ssize_t count = ::read(fd, chunk.data(), chunk.size());
      if (count < 0) {
        if (errno == EINTR) continue;
        throw std::system_error(errno, std::generic_category(), "read");
      }
#endif
      if (count == 0) break;
      std::string dataChunk(chunk.data(), static_cast<std::size_t>(count));
      if (firstChunk) {
        firstChunk = false;
        if (dataChunk.rfind("\xef\xbb\xbf", 0) == 0) dataChunk.erase(0, 3);
      }
      totalBytesRead += dataChunk.size();
      if (!truncateOnByteLimit && maxBytes && totalBytesRead > *maxBytes)
        tooLarge(totalBytesRead, *maxBytes, services);
      std::string data = partial.empty() ? std::move(dataChunk) : partial + dataChunk;
      partial.clear();
      std::size_t start = 0, newline;
      while ((newline = data.find('\n', start)) != std::string::npos) {
        if (lineIndex >= offset && lineIndex < endLine) {
          std::string line = data.substr(start, newline - start);
          if (!line.empty() && line.back() == '\r') line.pop_back();
          if (truncateOnByteLimit && maxBytes) {
            const std::size_t next = selectedBytes + (selected.empty() ? 0 : 1) + line.size();
            if (next > *maxBytes) { truncated = true; endLine = lineIndex; }
            else { selectedBytes = next; selected.push_back(std::move(line)); }
          } else selected.push_back(std::move(line));
        }
        ++lineIndex; start = newline + 1;
      }
      if (start < data.size() && lineIndex >= offset && lineIndex < endLine) {
        const std::string fragment = data.substr(start);
        if (truncateOnByteLimit && maxBytes) {
          const std::size_t fragBytes = selectedBytes + (selected.empty() ? 0 : 1) + fragment.size();
          if (fragBytes > *maxBytes) { truncated = true; endLine = lineIndex; }
          else partial = fragment;
        } else partial = fragment;
      }
    }
    std::string last = partial;
    if (!last.empty() && last.back() == '\r') last.pop_back();
    if (lineIndex >= offset && lineIndex < endLine) {
      if (truncateOnByteLimit && maxBytes) {
        const std::size_t next = selectedBytes + (selected.empty() ? 0 : 1) + last.size();
        if (next > *maxBytes) truncated = true;
        else selected.push_back(std::move(last));
      } else selected.push_back(std::move(last));
    }
    ++lineIndex;
#ifndef _WIN32
    ::close(fd);
#endif
    const std::string content = join(selected);
    return {content, selected.size(), lineIndex, totalBytesRead, content.size(), mtime, truncated};
  } catch (...) {
#ifndef _WIN32
    ::close(fd);
#endif
    throw;
  }
}

} // namespace batbox::filesystem
