#pragma once
#include "application/Config.hpp"
#include "runtime/JsonValue.hpp"
#include <filesystem>
#include <functional>
#include <string>

namespace batbox::application_bats {
bool batMemoryEnabled(const application::Config& config,
  const Json& appState,const std::filesystem::path& projectRoot);
std::string loadBatMemoryPoetry(const Json& definition,
  const application::Config& config,const Json& appState,
  const std::filesystem::path& projectRoot,
  const std::function<void(const std::string&)>& diagnostic);
std::string batSystemPoetryText(const Json& definition,
  const std::string& fallback,const application::Config& config,
  const Json& appState,const std::filesystem::path& projectRoot,
  const std::function<void(const std::string&)>& diagnostic);
}
