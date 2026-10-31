#pragma once
#include <optional>
#include <string>
#include <vector>

namespace batbox::application_editor::indigenous {
struct EditorCommand {
  std::vector<std::string> argv;
  std::string displayName;
  bool gui=false;
  // Windows launches the configured setting through cmd.exe (including
  // .cmd editor launchers and builtins); argv still supports classification.
  std::optional<std::string> windowsShellCommand=std::nullopt;
};
struct ProcessResult {
  int exitCode=0;
  std::optional<std::string> error;
};
std::optional<EditorCommand> externalEditor();
ProcessResult runInherited(const EditorCommand& command,
  const std::string& filePath);
bool launchDetached(const EditorCommand& command,
  const std::string& filePath,std::optional<int> line={});
}
