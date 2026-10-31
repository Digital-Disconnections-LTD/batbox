#pragma once
#include "application/Config.hpp"
#include "runtime/JsonValue.hpp"
#include <filesystem>
#include <functional>

namespace batbox::application_bats {
struct DefinitionInputs {
  application::Config config;
  std::filesystem::path projectRoot;
  std::function<void(const std::string&)> diagnostic;
};

// Resolves Bat definitions from built-ins, scoped Markdown files,
// JSON config bats, then active-name precedence. The result is stashed by the
// IndigenousBatApplication generation, which is replaced on roost rebind.
Json loadBatDefinitions(const DefinitionInputs& inputs);
Json parseMarkdownBat(const std::filesystem::path& file,
  const std::filesystem::path& baseDir,const std::string& source,
  const std::function<void(const std::string&)>& diagnostic);
Json parseJsonBat(const std::string& name,const Json& definition,
  const std::function<void(const std::string&)>& diagnostic);
}
