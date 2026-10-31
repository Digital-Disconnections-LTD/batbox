#pragma once
#include "shell/Heredoc.hpp"
#include <string>
#include <vector>

namespace batbox::shell_heredoc::detail {
struct Match {
  std::size_t start{},end{};
  bool dash{};
  char16_t quote{};
  std::u16string delimiter;
};
std::vector<Match> findStarts(const std::u16string& command);
std::size_t logicalNewline(const std::u16string& command,std::size_t from);
int closingLine(const std::u16string& command,std::size_t contentStart,
  const std::u16string& delimiter,bool dash,std::size_t& contentEnd);
std::vector<HeredocInfo> collect(const std::u16string& command,bool quotedOnly);
std::string salt(const SaltProvider& provider);
} // namespace batbox::shell_heredoc::detail
