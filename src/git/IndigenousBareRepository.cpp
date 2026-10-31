#include "git/BareRepository.hpp"
#include "runtime/Encoding.hpp"
#include <filesystem>
#include <stdexcept>
#include <system_error>

namespace batbox::git {
BareRepositoryImports indigenousBareRepositoryImports(std::function<std::string()> getCwd){
  BareRepositoryImports imports;
  imports.getCwd=std::move(getCwd);
  imports.statSync=[](const std::string& path){
    std::error_code error;
    const auto status=std::filesystem::status(value::encodeUtf8(path),error);
    if(error)throw std::system_error(error,"statSync");
    return BareStat{std::filesystem::is_regular_file(status),
      std::filesystem::is_directory(status)};
  };
  return imports;
}
} // namespace batbox::git
