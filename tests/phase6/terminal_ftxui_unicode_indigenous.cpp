#include "terminal_ftxui/IndigenousStringMetrics.hpp"
#include "terminal_ftxui/PresentationView.hpp"
#include "ftxui/screen/screen.hpp"
#include "ftxui/screen/string.hpp"
#include <cassert>
#include <iostream>

namespace ix = batbox::interactive;
namespace tf = batbox::terminal_ftxui;

int main() {
  auto intl = std::make_shared<batbox::intl::Stash>(batbox::intl::indigenousServices());
  auto width = std::make_shared<batbox::text_width::StringWidth>(
      batbox::text_width::indigenousServices(intl));
  tf::installIndigenousStringMetrics(intl, width);

  assert(ftxui::string_width("👩‍💻") == 2);
  assert(ftxui::string_width("👨‍👩‍👧‍👦") == 2);
  assert(ftxui::string_width("👍🏽") == 2);
  assert(ftxui::string_width("🇺🇸") == 2);
  assert(ftxui::string_width("界") == 2);
  assert(ftxui::string_width("é") == 1);
  const auto glyphs = ftxui::Utf8ToGlyphs("A👩‍💻B");
  assert(glyphs.size() == 4 && glyphs[0] == "A" &&
         glyphs[1] == "👩‍💻" && glyphs[2].empty() && glyphs[3] == "B");

  ix::PresentationState state;
  state.transcriptRows.push_back({ix::TranscriptRow::Kind::Assistant,
      "a", "A👩‍💻B", false});
  state.input.text = "👩‍💻x";
  state.input.caretUtf16 = 5;
  ftxui::Screen screen(8, 3);
  ftxui::Render(screen, tf::renderPresentation(state, {8, 3}));
  assert(screen.CellAt(2, 0).character == "A");
  assert(screen.CellAt(3, 0).character == "👩‍💻");
  assert(screen.CellAt(4, 0).character.empty());
  assert(screen.CellAt(5, 0).character == "B");
  assert(screen.ToString().find("A👩‍💻B") != std::string::npos);
  assert(screen.cursor().x == 4 && screen.cursor().y == 2);
  const auto wrapped = tf::layoutPresentation(state, 5, 3);
  assert(wrapped.transcriptRowCount == 2);
  assert(wrapped.bodyHeight == 2);
  std::cout << "13 native joined-emoji width, cell, wrap and caret checks pass\n";
}
