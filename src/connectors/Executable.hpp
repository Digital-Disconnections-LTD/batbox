#pragma once
#include <filesystem>
#include <optional>
#include <string>

namespace batbox::connectors {
// Resolves a configured helper executable without starting it. A name with a
// directory separator is checked as a path; a bare name is searched on PATH
// (with PATHEXT on Windows).
std::optional<std::filesystem::path> findExecutable(const std::string& command);
} // namespace batbox::connectors
