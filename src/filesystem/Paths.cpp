#include "filesystem/FileOps.hpp"
#include "runtime/IndigenousPath.hpp"
#include <filesystem>
#include <stdexcept>
#include <cerrno>
#include <system_error>
#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace batbox::filesystem {
namespace fs = std::filesystem;
namespace {
std::string normalized(const std::string& path) {
  const std::string value = runtime::pathToUtf8(
    runtime::pathFromUtf8(path).lexically_normal());
  return value.empty() ? "." : value;
}
std::string nfc(const std::string& path, const PathServices& services) {
  if (!services.normalizeNFC) throw std::invalid_argument("Missing path binding: normalizeNFC");
  return services.normalizeNFC(path);
}
} // namespace

std::string expandPath(const std::string& path, const PathServices& services,
                       const std::optional<std::string>& baseDir) {
  const std::string actualBaseDir = baseDir ? *baseDir :
    (services.getCwd ? services.getCwd() : runtime::pathToUtf8(fs::current_path()));
  if (path.find('\0') != std::string::npos || actualBaseDir.find('\0') != std::string::npos)
    throw std::invalid_argument("Path contains null bytes");
  const std::string trimmed = value::trim(path);
  if (trimmed.empty()) return nfc(normalized(actualBaseDir), services);
  if (trimmed == "~") {
    if (!services.homeDirectory) throw std::invalid_argument("Missing path binding: homeDirectory");
    return nfc(services.homeDirectory(), services);
  }
  if (trimmed.rfind("~/", 0) == 0) {
    if (!services.homeDirectory) throw std::invalid_argument("Missing path binding: homeDirectory");
    return nfc(normalized(runtime::pathToUtf8(runtime::pathFromUtf8(services.homeDirectory()) /
      runtime::pathFromUtf8(trimmed.substr(2)))), services);
  }
  std::string processed = trimmed;
  if (services.platform && services.platform() == "windows" && trimmed.size() >= 3 &&
      trimmed[0] == '/' && ((trimmed[1] >= 'a' && trimmed[1] <= 'z') ||
                            (trimmed[1] >= 'A' && trimmed[1] <= 'Z')) && trimmed[2] == '/') {
    if (services.posixPathToWindowsPath) {
      try { processed = services.posixPathToWindowsPath(trimmed); }
      catch (...) { processed = trimmed; }
    }
  }
  if (runtime::pathFromUtf8(processed).is_absolute()) return nfc(normalized(processed), services);
  return nfc(normalized(runtime::pathToUtf8(fs::absolute(runtime::pathFromUtf8(actualBaseDir) /
    runtime::pathFromUtf8(processed)))), services);
}

ResolvedPath safeResolvePath(const FsView& view,const std::string& filePath) {
  if (filePath.rfind("//", 0) == 0 || filePath.rfind("\\\\", 0) == 0)
    return {filePath, false, false};
  try {
    const auto fileStat=view.lstatSync(filePath);
    if(fileStat.fifo||fileStat.socket||fileStat.characterDevice||fileStat.blockDevice)
      return {filePath,false,false};
    const std::string resolved=view.realpathSync(filePath);
    return {resolved, resolved != filePath, true};
  } catch (...) { return {filePath, false, false}; }
}

FsView indigenousFileView(){
  FsView view;
  view.existsSync=[](const std::string& path){
    std::error_code error;
    return fs::exists(runtime::pathFromUtf8(path),error);
  };
  view.lstatSync=[](const std::string& path){
#ifdef _WIN32
    const auto status=fs::symlink_status(runtime::pathFromUtf8(path));
    return FsStat{fs::is_fifo(status),fs::is_socket(status),
      fs::is_character_file(status),fs::is_block_file(status),fs::is_symlink(status)};
#else
    struct stat info{};
    if(::lstat(path.c_str(),&info)<0)
      throw std::system_error(errno,std::generic_category(),"lstat");
    return FsStat{S_ISFIFO(info.st_mode),S_ISSOCK(info.st_mode),
      S_ISCHR(info.st_mode),S_ISBLK(info.st_mode),S_ISLNK(info.st_mode)};
#endif
  };
  view.realpathSync=[](const std::string& path){return runtime::pathToUtf8(
    fs::canonical(runtime::pathFromUtf8(path)));};
  view.readlinkSync=[](const std::string& path){
#ifdef _WIN32
    return runtime::pathToUtf8(fs::read_symlink(runtime::pathFromUtf8(path)));
#else
    std::string target(256,'\0');
    for(;;){
      const auto count=::readlink(path.c_str(),target.data(),target.size());
      if(count<0)throw std::system_error(errno,std::generic_category(),"readlink");
      if(static_cast<std::size_t>(count)<target.size()){
        target.resize(static_cast<std::size_t>(count));return target;
      }
      target.resize(target.size()*2);
    }
#endif
  };
  return view;
}

ResolvedPath safeResolvePath(const std::string& filePath){
  static const FsView view=indigenousFileView();
  return safeResolvePath(view,filePath);
}

} // namespace batbox::filesystem
