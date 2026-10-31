#pragma once
#include <exception>
#include <filesystem>
#include <functional>
#include <string>

namespace batbox::bats {
std::string initializeMissionOutputAsSymlink(const std::filesystem::path& output,
  const std::filesystem::path& target,
  const std::function<void(const std::exception&)>& logError = {});
}
