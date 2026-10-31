#pragma once

#include <optional>
#include <string>

namespace batbox::interactive::cards {

struct BashInputLeaf {
  std::string input;
  bool addMargin = false;
};
struct BashOutputLeaf {
  std::string stdoutText;
  std::string stderrText;
  bool verbose = false;
};
struct CommandLeaf {
  std::string label;
  bool skill = false;
  bool addMargin = false;
};

std::optional<BashInputLeaf> bashInputLeaf(const std::string& text,
                                           bool addMargin);
BashOutputLeaf bashOutputLeaf(const std::string& text, bool verbose);
std::optional<CommandLeaf> commandLeaf(const std::string& text,
                                       const std::string& commandTag,
                                       bool addMargin);

} // namespace batbox::interactive::cards
