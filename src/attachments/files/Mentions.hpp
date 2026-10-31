#pragma once
#include <optional>
#include <string>
#include <vector>

namespace batbox::attachments::files {
struct FileMention {
  std::string filename;
  std::optional<double> lineStart,lineEnd;
};
std::vector<std::string> extractAtMentionedFiles(const std::string& content);
FileMention parseAtMentionedFileLines(const std::string& mention);
} // namespace batbox::attachments::files
