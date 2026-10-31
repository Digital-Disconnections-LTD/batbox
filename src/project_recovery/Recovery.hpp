#pragma once
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace batbox::project_recovery {
inline constexpr std::size_t stateFileByteLimit=2048;
inline constexpr std::size_t makefileByteLimit=2048;
inline constexpr std::size_t packetByteLimit=8192;
inline constexpr std::size_t topicIndexByteLimit=1024;

struct RecoveryPacket {
  std::filesystem::path projectRoot,directory;
  std::string text;
  std::vector<std::filesystem::path> files;
  std::vector<std::string> warnings;
};
struct InitResult {
  std::filesystem::path projectRoot,directory;
  std::vector<std::filesystem::path> created,existing;
  std::vector<std::string> warnings;
};

// Read-only discovery; a .git directory or worktree file bounds the search.
std::optional<std::filesystem::path> findDirectory(const std::filesystem::path& cwd);
// Reads Makefile, 00_project.md and 10_memory.md; other numbered files are indexed.
// Empty when no recovery directory exists. Never evaluates Makefile recipes.
RecoveryPacket load(const std::filesystem::path& cwd);
// Explicit opt-in only. Create missing defaults at the nearest Git root or cwd.
InitResult init(const std::filesystem::path& cwd);
}
