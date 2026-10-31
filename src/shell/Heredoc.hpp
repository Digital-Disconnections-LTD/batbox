#pragma once
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace batbox::shell_heredoc {
struct HeredocInfo {
  std::string fullText;
  std::string delimiter;
  std::size_t operatorStartIndex{};
  std::size_t operatorEndIndex{};
  std::size_t contentStartIndex{};
  std::size_t contentEndIndex{};
};
struct Entry { std::string placeholder; HeredocInfo info; };
struct ExtractionResult {
  std::string processedCommand;
  std::vector<Entry> heredocs; // insertion order is Map insertion order
};
using SaltProvider=std::function<std::string()>;
ExtractionResult extractHeredocs(const std::string& command,bool quotedOnly=false,
  const SaltProvider& saltProvider={});
std::vector<std::string> restoreHeredocs(const std::vector<std::string>& parts,
  const std::vector<Entry>& heredocs);
bool containsHeredoc(const std::string& command);
} // namespace batbox::shell_heredoc
