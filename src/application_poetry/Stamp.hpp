#pragma once
#include <chrono>
#include <filesystem>
#include <string>

namespace batbox::application_poetry {
// Modification time (seconds and sub-second ticks) and size, or "missing".
// A path whose size cannot be read (a directory) reports "directory".
inline std::string fileStamp(const std::filesystem::path& path){
  std::error_code error;
  const auto time=std::filesystem::last_write_time(path,error);
  if(error)return "missing";
  const auto size=std::filesystem::file_size(path,error);
  const auto ticks=time.time_since_epoch();
  const auto seconds=std::chrono::duration_cast<std::chrono::seconds>(ticks);
  return std::to_string(seconds.count())+":"+
    std::to_string(static_cast<long long>((ticks-seconds).count()))+":"+
    (error?std::string("directory"):std::to_string(size));
}
} // namespace batbox::application_poetry
