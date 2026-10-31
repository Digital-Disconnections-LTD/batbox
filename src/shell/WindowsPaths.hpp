#pragma once
#include <optional>
#include <string>

namespace batbox::shell {
// Git Bash provides the Bash Gadget shell on Windows.
std::string findGitBashPath();
std::optional<std::string> findPowerShellPath();
std::string windowsPathToPosixPath(const std::string& path);
std::string posixPathToWindowsPath(const std::string& path);
}
