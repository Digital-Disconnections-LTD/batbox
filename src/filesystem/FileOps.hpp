#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <stop_token>
#include <string>

namespace batbox::filesystem {

struct PathServices {
  std::function<std::string()> getCwd;
  std::function<std::string()> homeDirectory;
  std::function<std::string()> platform;
  std::function<std::string(const std::string&)> posixPathToWindowsPath;
  std::function<std::string(const std::string&)> normalizeNFC;
};

struct ResolvedPath {
  std::string resolvedPath;
  bool isSymlink = false;
  bool isCanonical = false;
};
struct FsStat {
  bool fifo=false,socket=false,characterDevice=false,blockDevice=false,symbolicLink=false;
};
struct FsView {
  std::function<bool(const std::string&)> existsSync;
  std::function<FsStat(const std::string&)> lstatSync;
  std::function<std::string(const std::string&)> realpathSync;
  std::function<std::string(const std::string&)> readlinkSync;
};
struct FileMetadata {
  std::string content;
  std::string encoding;
  std::string lineEndings;
};

std::string expandPath(const std::string& path, const PathServices& services,
                       const std::optional<std::string>& baseDir = std::nullopt);
ResolvedPath safeResolvePath(const std::string& filePath);
ResolvedPath safeResolvePath(const FsView& fs,const std::string& filePath);
FsView indigenousFileView();
std::string detectEncodingForResolvedPath(const std::string& resolvedPath);
std::string detectLineEndingsForString(const std::string& content);
FileMetadata readFileSyncWithMetadata(const std::string& filePath);
std::string readFileBytes(const std::string& filePath,
  std::optional<std::size_t> maxBytes = std::nullopt, std::stop_token stopToken = {});
double getFileModificationTime(const std::string& filePath);
double getFileModificationTimeRaw(const std::string& filePath);
void writeTextContent(const std::string& filePath, const std::string& content,
                      const std::string& encoding, const std::string& endings);

} // namespace batbox::filesystem
