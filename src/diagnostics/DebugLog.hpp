#pragma once
#include "diagnostics/DebugFilter.hpp"
#include "diagnostics/DebugLogOptions.hpp"
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <optional>
#include <string>

namespace batbox::diagnostics {
// A copy of the application log for one run: to stderr, to the file named on
// the command line, or to debug/<Flight id>.txt beside a "latest" link.
class DebugLog {
public:
  DebugLog(DebugLogOptions options,std::filesystem::path defaultFile,
    std::function<std::string()> timestamp,
    std::function<void(const std::string&)> writeToStderr);
  const std::filesystem::path& path()const{return path_;}
  bool toStderr()const{return options_.toStderr;}
  // Safe from any thread. Lines the filter excludes are dropped.
  void write(const std::string& message);
private:
  DebugLogOptions options_;
  std::optional<DebugFilter> filter_;
  std::filesystem::path path_;
  std::function<std::string()> timestamp_;
  std::function<void(const std::string&)> writeToStderr_;
  std::mutex mutex_;
  std::ofstream file_;
  bool opened_=false;
};
}
