#pragma once
#include <filesystem>
#include <optional>
#include <string>

namespace batbox::diagnostics {
// What the command line asked for. Nothing is written unless enabled.
struct DebugLogOptions {
  bool enabled=false;
  bool toStderr=false;
  std::optional<std::filesystem::path> file;
  std::optional<std::string> filter;
};
}
