#pragma once
#include <filesystem>
#include <string>

namespace batbox::runtime {
inline std::filesystem::path pathFromUtf8(const std::string& utf8){
  return std::filesystem::path(std::u8string(
    reinterpret_cast<const char8_t*>(utf8.data()),utf8.size()));
}
inline std::string pathToUtf8(const std::filesystem::path& path){
  const auto bytes=path.u8string();
  return std::string(reinterpret_cast<const char*>(bytes.data()),bytes.size());
}
} // namespace batbox::runtime
