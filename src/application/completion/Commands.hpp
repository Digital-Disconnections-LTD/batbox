#pragma once
#include "application/completion/Completion.hpp"
#include <optional>

namespace batbox::application::completion {
struct CommandSpan {
  std::string query;
  std::size_t startUtf16=0,endUtf16=0;
  bool atStart=false;
};
std::optional<CommandSpan> findCommandSpan(
  const std::string& text,std::size_t caretUtf16);
std::vector<Suggestion> commandSuggestions(
  const CommandSpan& span,const Json& commands);
} // namespace batbox::application::completion
