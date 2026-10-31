#pragma once

#include <functional>
#include <optional>
#include <string>

namespace batbox::interactive::cards {

struct OutputLineImports {
  std::function<double(const std::string&)> stringWidth;
  std::function<std::string(const std::string&, double, double)> sliceAnsi;
  std::function<std::string(const std::string&)> dim;
  std::function<std::string()> expandHint;
  std::function<std::string(const std::string&)> createHyperlink;
};
struct OutputLineOptions {
  int columns = 80;
  bool verbose = false;
  bool expandShellOutput = false;
  bool inVirtualList = false;
  bool linkifyUrls = false;
  bool isError = false;
  bool isWarning = false;
};
struct OutputLineCard {
  std::string formatted;
  std::optional<std::string> colorRole;
};

std::string tryFormatJson(const std::string& line);
std::string tryJsonFormatContent(const std::string& content);
std::string linkifyUrlsInText(const std::string& content,
                              const OutputLineImports& imports);
std::string stripUnderlineAnsi(const std::string& content);
std::string renderTruncatedContent(const std::string& content,
                                    int terminalWidth, bool suppressExpandHint,
                                    const OutputLineImports& imports);
OutputLineCard outputLineCard(const std::string& content,
                              const OutputLineOptions& options,
                              const OutputLineImports& imports);

} // namespace batbox::interactive::cards
