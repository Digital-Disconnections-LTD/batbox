#pragma once
#include "application_completion/Completion.hpp"
#include <optional>

namespace batbox::application_completion {
struct PathSpan {
  std::string partial,prefix;
  std::size_t startUtf16=0,endUtf16=0;
  bool includeFiles=true;
};
std::optional<PathSpan> findPathSpan(
  const std::string& text,std::size_t caretUtf16);
} // namespace batbox::application_completion
