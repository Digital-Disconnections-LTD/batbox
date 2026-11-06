#pragma once

#include <ftxui/dom/elements.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace batbox::interactive::markdown {
ftxui::Element projectAnsi(const std::string& source,bool dim=false);

struct AnsiStyle {
  bool bold=false,italic=false,underline=false,dim=false,strike=false;
  std::optional<ftxui::Color> foreground,background;
  std::string link;
};
// projectAnsi for one stretch of a longer text: SGR state starts from `style`
// and is left in it, so consecutive stretches split at newlines draw the
// lines projectAnsi draws for the whole text.
ftxui::Element projectAnsiPart(std::string_view source,bool dim,AnsiStyle& style);
struct AnsiRun {
  std::string text;
  AnsiStyle style;
};
// One entry per source line; SGR state carries across lines like a terminal.
std::vector<std::vector<AnsiRun>> parseAnsiLines(const std::string& source);
// The color an SGR sequence such as "\x1b[38;2;1;2;3m" selects, if any.
std::optional<ftxui::Color> sgrColor(const std::string& sequence,bool background=false);
ftxui::Element styleAnsiRun(ftxui::Element element,const AnsiStyle& style);
} // namespace batbox::interactive::markdown
