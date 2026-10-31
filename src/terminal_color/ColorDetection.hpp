#pragma once
#include "terminal_color/ChalkIndigenous.hpp"
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace batbox::terminal_color {
struct ColorDetectionInputs {
  std::function<std::optional<std::string>(const std::string&)> getenv;
  std::vector<std::string> argv;
  std::string platform;
  std::string osRelease;
  bool haveStream=true;
  bool streamIsTTY=false;
  bool sniffFlags=true;
};
double detectColorLevel(const ColorDetectionInputs& inputs);
ColorDetectionInputs indigenousColorDetectionInputs(
  const std::vector<std::string>& argv);
ChalkState initializeIndigenousChalkState(
  const std::vector<std::string>& argv,bool tmuxTruecolor=false);
} // namespace batbox::terminal_color
