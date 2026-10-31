#include "filesystem/FileOps.hpp"
#include "runtime/IndigenousPath.hpp"
#include "runtime/IndigenousStatTime.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cerrno>
#include <filesystem>
#include <stdexcept>
#include <string_view>
#include <system_error>
#ifndef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#else
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace batbox::filesystem {
namespace fs = std::filesystem;
namespace {
[[noreturn]] void osError(const char* operation) {
#ifdef _WIN32
  throw std::system_error(static_cast<int>(::GetLastError()),std::system_category(),operation);
#else
  throw std::system_error(errno, std::generic_category(), operation);
#endif
}
std::string readBytes(const std::string& path, std::size_t limit = static_cast<std::size_t>(-1),
                      std::stop_token stopToken = {}) {
#ifdef _WIN32
  const auto indigenous=runtime::pathFromUtf8(path);
  HANDLE file=::CreateFileW(indigenous.c_str(),GENERIC_READ,
    FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,
    FILE_ATTRIBUTE_NORMAL,nullptr);
  if(file==INVALID_HANDLE_VALUE)osError("open");
  std::string data;char chunk[8192];
  try{
    while(data.size()<limit){
      if(stopToken.stop_requested())throw std::runtime_error("This operation was aborted");
      const auto wanted=static_cast<DWORD>(std::min(sizeof(chunk),limit-data.size()));
      DWORD count=0;
      if(!::ReadFile(file,chunk,wanted,&count,nullptr))osError("read");
      if(count==0)break;
      data.append(chunk,count);
    }
  }catch(...){::CloseHandle(file);throw;}
  ::CloseHandle(file);return data;
#else
  const int fd = ::open(path.c_str(), O_RDONLY);
  if (fd < 0) osError("open");
  std::string data;
  char chunk[8192];
  try {
    while (data.size() < limit) {
      if (stopToken.stop_requested()) throw std::runtime_error("This operation was aborted");
      const std::size_t wanted = std::min(sizeof(chunk), limit - data.size());
      const ssize_t count = ::read(fd, chunk, wanted);
      if (count < 0) { if (errno == EINTR) continue; osError("read"); }
      if (count == 0) break;
      data.append(chunk, static_cast<std::size_t>(count));
    }
  } catch (...) { ::close(fd); throw; }
  ::close(fd);
  return data;
#endif
}
#ifndef _WIN32
void writeBytes(int fd, std::string_view data) {
  std::size_t offset = 0;
  while (offset < data.size()) {
    const ssize_t count = ::write(fd, data.data() + offset, data.size() - offset);
    if (count < 0) { if (errno == EINTR) continue; osError("write"); }
    offset += static_cast<std::size_t>(count);
  }
  if (::fsync(fd) != 0) osError("fsync");
}
#endif
std::string replaceAll(std::string text, const std::string& before, const std::string& after) {
  std::size_t pos = 0;
  while ((pos = text.find(before, pos)) != std::string::npos) {
    text.replace(pos, before.size(), after); pos += after.size();
  }
  return text;
}
void appendUtf8(std::string& out, char32_t codepoint) {
  if (codepoint < 0x80) out += static_cast<char>(codepoint);
  else if (codepoint < 0x800) {
    out += static_cast<char>(0xc0 | (codepoint >> 6));
    out += static_cast<char>(0x80 | (codepoint & 0x3f));
  } else if (codepoint < 0x10000) {
    out += static_cast<char>(0xe0 | (codepoint >> 12));
    out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f));
    out += static_cast<char>(0x80 | (codepoint & 0x3f));
  } else {
    out += static_cast<char>(0xf0 | (codepoint >> 18));
    out += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f));
    out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f));
    out += static_cast<char>(0x80 | (codepoint & 0x3f));
  }
}
std::string encodeUtf8USV(std::string_view text) {
  const auto units = value::utf16CodeUnits(text);
  std::string result;
  for (std::size_t i = 0; i < units.size(); ++i) {
    const char16_t unit = units[i];
    if (unit >= 0xd800 && unit <= 0xdbff && i + 1 < units.size() &&
        units[i + 1] >= 0xdc00 && units[i + 1] <= 0xdfff) {
      const char32_t cp = 0x10000 + ((unit - 0xd800) << 10) + (units[++i] - 0xdc00);
      appendUtf8(result, cp);
    } else appendUtf8(result, unit >= 0xd800 && unit <= 0xdfff ? 0xfffd : unit);
  }
  return result;
}
std::string encode(const std::string& content, const std::string& encoding) {
  if (encoding == "utf16le") {
    std::string result;
    for (const char16_t unit : value::utf16CodeUnits(content)) {
      result += static_cast<char>(unit & 0xff);
      result += static_cast<char>(unit >> 8);
    }
    return result;
  }
  if (encoding == "utf8" || encoding == "utf-8") return encodeUtf8USV(content);
  throw std::invalid_argument("Unsupported file encoding: " + encoding);
}
std::string decode(const std::string& bytes, const std::string& encoding) {
  if (encoding == "utf16le") {
    std::u16string units;
    for (std::size_t i = 0; i + 1 < bytes.size(); i += 2)
      units.push_back(static_cast<char16_t>(static_cast<unsigned char>(bytes[i]) |
        (static_cast<unsigned char>(bytes[i + 1]) << 8)));
    return value::fromUtf16(units);
  }
  return bytes;
}
void writeAt(const std::string& path, std::string_view bytes, unsigned int mode, bool setMode) {
#ifdef _WIN32
  (void)mode;(void)setMode;
  const auto indigenous=runtime::pathFromUtf8(path);
  HANDLE file=::CreateFileW(indigenous.c_str(),GENERIC_WRITE,
    FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,CREATE_ALWAYS,
    FILE_ATTRIBUTE_NORMAL,nullptr);
  if(file==INVALID_HANDLE_VALUE)osError("open for write");
  try{
    for(std::size_t offset=0;offset<bytes.size();){
      const auto chunk=static_cast<DWORD>(std::min<std::size_t>(bytes.size()-offset,0xffffffffu));
      DWORD written=0;
      if(!::WriteFile(file,bytes.data()+offset,chunk,&written,nullptr))osError("write");
      if(written==0)throw std::runtime_error("Short file write");
      offset+=written;
    }
    if(!::FlushFileBuffers(file))osError("fsync");
  }catch(...){::CloseHandle(file);throw;}
  if(!::CloseHandle(file))osError("close");
#else
  const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, mode);
  if (fd < 0) osError("open for write");
  try {
    writeBytes(fd, bytes);
    if (setMode && ::fchmod(fd, mode) != 0) osError("fchmod");
  } catch (...) { ::close(fd); throw; }
  if (::close(fd) != 0) osError("close");
#endif
}
void atomicWrite(const std::string& filePath, const std::string& bytes) {
  std::string target = filePath;
  std::error_code ec;
  const auto indigenousFile=runtime::pathFromUtf8(filePath);
  if (fs::is_symlink(fs::symlink_status(indigenousFile, ec))) {
    const fs::path link = fs::read_symlink(indigenousFile);
    target = runtime::pathToUtf8(link.is_absolute() ? link :
      (indigenousFile.parent_path() / link).lexically_normal());
  }
#ifdef _WIN32
  const auto indigenousTarget=runtime::pathFromUtf8(target);
  const bool existed=fs::exists(indigenousTarget,ec);
  if(ec)throw std::system_error(ec,"stat before write");
  const auto now=std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
  const std::string temp=target+".tmp."+std::to_string(::GetCurrentProcessId())+"."+
    std::to_string(now);
  try{
    writeAt(temp,bytes,0666,existed);
    if(!::MoveFileExW(runtime::pathFromUtf8(temp).c_str(),indigenousTarget.c_str(),
       MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))osError("rename");
  }catch(...){
    std::error_code ignored;fs::remove(runtime::pathFromUtf8(temp),ignored);
    writeAt(target,bytes,0666,existed);
  }
#else
  struct stat targetStat {};
  const bool existed = ::stat(target.c_str(), &targetStat) == 0;
  if (!existed && errno != ENOENT) osError("stat before write");
  const mode_t mode = existed ? targetStat.st_mode : 0666;
  const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
  const std::string temp = target + ".tmp." + std::to_string(::getpid()) + "." + std::to_string(now);
  try {
    writeAt(temp, bytes, mode, existed);
    if (::rename(temp.c_str(), target.c_str()) != 0) osError("rename");
  } catch (...) {
    ::unlink(temp.c_str());
    writeAt(target, bytes, mode, existed);
  }
#endif
}
} // namespace

std::string detectEncodingForResolvedPath(const std::string& resolvedPath) {
  const std::string head = readBytes(resolvedPath, 4096);
  if (head.size() >= 2 && static_cast<unsigned char>(head[0]) == 0xff &&
      static_cast<unsigned char>(head[1]) == 0xfe) return "utf16le";
  return "utf8";
}

std::string readFileBytes(const std::string& filePath,
  std::optional<std::size_t> maxBytes, std::stop_token stopToken) {
  return readBytes(filePath, maxBytes.value_or(static_cast<std::size_t>(-1)), stopToken);
}

std::string detectLineEndingsForString(const std::string& content) {
  std::size_t crlf = 0, lf = 0;
  for (std::size_t i = 0; i < content.size(); ++i) {
    if (content[i] == '\n') {
      if (i > 0 && content[i - 1] == '\r') ++crlf;
      else ++lf;
    }
  }
  return crlf > lf ? "CRLF" : "LF";
}

FileMetadata readFileSyncWithMetadata(const std::string& filePath) {
  const ResolvedPath path = safeResolvePath(filePath);
  const std::string encoding = detectEncodingForResolvedPath(path.resolvedPath);
  const std::string raw = decode(readBytes(path.resolvedPath), encoding);
  return {replaceAll(raw, "\r\n", "\n"), encoding,
    detectLineEndingsForString(value::slice(raw, 0, 4096))};
}

double getFileModificationTime(const std::string& filePath) {
  return std::floor(getFileModificationTimeRaw(filePath));
}

double getFileModificationTimeRaw(const std::string& filePath) {
#ifdef _WIN32
  WIN32_FILE_ATTRIBUTE_DATA info{};
  if(!::GetFileAttributesExW(runtime::pathFromUtf8(filePath).c_str(),
       GetFileExInfoStandard,&info))osError("stat");
  ULARGE_INTEGER ticks{};ticks.LowPart=info.ftLastWriteTime.dwLowDateTime;
  ticks.HighPart=info.ftLastWriteTime.dwHighDateTime;
  return static_cast<double>(ticks.QuadPart-116444736000000000ULL)/10000.0;
#else
  struct stat fileStat {};
  if (::stat(filePath.c_str(), &fileStat) != 0) osError("stat");
  return runtime::statMtimeMillis(fileStat);
#endif
}

void writeTextContent(const std::string& filePath, const std::string& content,
                      const std::string& encoding, const std::string& endings) {
  std::string toWrite = content;
  if (endings == "CRLF") {
    toWrite = replaceAll(toWrite, "\r\n", "\n");
    toWrite = replaceAll(toWrite, "\n", "\r\n");
  }
  atomicWrite(filePath, encode(toWrite, encoding));
}

} // namespace batbox::filesystem
