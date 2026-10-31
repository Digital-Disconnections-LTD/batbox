#pragma once
#include <filesystem>
#include <optional>
#include <string>

namespace batbox::application_lsp {
std::optional<std::string> symbolAtPosition(const std::filesystem::path& path,
  int zeroBasedLine,int zeroBasedCharacter);
}
