#pragma once
#include "runtime/JsonValue.hpp"
#include <filesystem>
#include <string>

namespace batbox::application_lsp {
struct Formatted {std::string text;int resultCount=0,fileCount=0;};
Formatted formatResult(const std::string& operation,const Json& result,
  const std::filesystem::path& roost);
}
