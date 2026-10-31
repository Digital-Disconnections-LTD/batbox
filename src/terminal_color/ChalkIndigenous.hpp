#pragma once
#include <optional>
#include <string>

namespace batbox::terminal_color {
// Indigenous implementation of Chalk 5.6.2 builder behavior.
struct ChalkState {
  double level=0;
  bool boostedForXtermJs=false;
  bool clampedForTmux=false;
};
ChalkState initializeChalkState(double detectedLevel,
  const std::optional<std::string>& termProgram,
  const std::optional<std::string>& tmux,
  bool tmuxTruecolor);
std::string chalkNamed(const std::string& text,const std::string& name,
  double level);
std::string chalkAnsi256(const std::string& text,double code,bool background,
  double level);
std::string chalkRgb(const std::string& text,double red,double green,
  double blue,bool background,double level);
std::string chalkHex(const std::string& text,const std::string& hex,
  bool background,double level);
} // namespace batbox::terminal_color
