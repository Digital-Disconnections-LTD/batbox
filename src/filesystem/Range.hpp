#pragma once
#include <cstddef>
#include <functional>
#include <optional>
#include <stop_token>
#include <stdexcept>
#include <string>
#include <system_error>

namespace batbox::filesystem {

struct RangeError : std::runtime_error {
  std::errc code;
  RangeError(std::errc errorCode,const std::string& message)
    :std::runtime_error(message),code(errorCode){}
};
struct FileTooLargeError : std::runtime_error {
  std::size_t sizeInBytes,maxSizeBytes;
  FileTooLargeError(std::size_t actual,std::size_t maximum,
    const std::string& message):std::runtime_error(message),
    sizeInBytes(actual),maxSizeBytes(maximum){}
};

struct ReadFileRangeResult {
  std::string content;
  std::size_t lineCount = 0;
  std::size_t totalLines = 0;
  std::size_t totalBytes = 0;
  std::size_t readBytes = 0;
  double mtimeMs = 0;
  bool truncatedByBytes = false;
};
struct RangeServices {
  std::function<std::string(double)> formatFileSize;
};

ReadFileRangeResult readFileInRangeFast(const std::string& raw, double mtimeMs,
  std::size_t offset, std::optional<std::size_t> maxLines,
  std::optional<std::size_t> truncateAtBytes);
ReadFileRangeResult readFileInRangeStreaming(const std::string& filePath,
  std::size_t offset, std::optional<std::size_t> maxLines,
  std::optional<std::size_t> maxBytes, bool truncateOnByteLimit,
  std::stop_token stopToken, const RangeServices& services);
ReadFileRangeResult readFileInRange(const std::string& filePath,
  std::size_t offset = 0, std::optional<std::size_t> maxLines = std::nullopt,
  std::optional<std::size_t> maxBytes = std::nullopt,
  std::stop_token stopToken = {}, bool truncateOnByteLimit = false,
  const RangeServices& services = {});

} // namespace batbox::filesystem
