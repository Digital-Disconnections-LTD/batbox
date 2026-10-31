#include "interactive/cards/UserTextProjection.hpp"

namespace batbox::interactive::cards {
namespace {
ftxui::Element margin(ftxui::Element content, bool addMargin) {
  if (!addMargin) return content;
  return ftxui::vbox({ftxui::text(""), content});
}
} // namespace

ftxui::Element projectBashInput(const BashInputLeaf& leaf,
                                const UserCardPalette& palette) {
  auto row = ftxui::hbox({
    ftxui::text("! ") | ftxui::color(palette.bashBorder),
    ftxui::text(leaf.input) | ftxui::color(palette.text),
    ftxui::text(" ")
  }) | ftxui::bgcolor(palette.bashBackground);
  return margin(std::move(row), leaf.addMargin);
}

ftxui::Element projectCommand(const CommandLeaf& leaf,
                              const UserCardPalette& palette,
                              const std::string& pointerGlyph) {
  auto row = ftxui::hbox({
    ftxui::text(pointerGlyph + " ") | ftxui::color(palette.subtle),
    ftxui::text(leaf.label) | ftxui::color(palette.text),
    ftxui::text(" ")
  }) | ftxui::bgcolor(palette.userBackground);
  return margin(std::move(row), leaf.addMargin);
}
} // namespace batbox::interactive::cards
