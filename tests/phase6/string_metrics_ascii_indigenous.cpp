#include "terminal_ftxui/IndigenousStringMetrics.hpp"
#include "ftxui/screen/string.hpp"
#include <cassert>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace tf = batbox::terminal_ftxui;
using batbox::Json;

int main() {
  auto intl = std::make_shared<batbox::intl::Stash>(batbox::intl::indigenousServices());
  auto width = std::make_shared<batbox::text_width::StringWidth>(
      batbox::text_width::indigenousServices(intl));
  tf::installIndigenousStringMetrics(intl, width);
  const auto segmenter = intl->getGraphemeSegmenter();

  std::vector<std::string> corpus = {
    "", " ", "a", "plain ascii text", "tabs\tand\tstops", "line\nbreak", "\n",
    "\r\n", "crlf\r\nline", "lone\rreturn", "trailing\r", "\x01\x02 controls \x1f",
    "\x1b[31mred\x1b[0m", "\x7f delete", "~!@#$%^&*()_+{}|:\"<>?`-=[]\\;',./",
    "é", "naïve café", "界", "日本語テキスト", "한국어", "👩‍💻", "👨‍👩‍👧‍👦", "👍🏽", "🇺🇸🇯🇵",
    "A👩‍💻B", "é", "mixed ascii then 界 then ascii", "zero​width",
    " nbsp", "ascii\n界\n👍🏽\r\n", std::string(4096, 'x'),
  };
  std::mt19937 random(7);
  const std::string alphabet = "abcXYZ019 \t\n\r\x01\x1b{}[]\"'\\/";
  for (int i = 0; i < 500; ++i) {
    std::string text;
    const auto length = random() % 40;
    for (std::size_t j = 0; j < length; ++j) text += alphabet[random() % alphabet.size()];
    if (i % 5 == 0) text += "界";
    if (i % 7 == 0) text += "👩‍💻";
    if (i % 11 == 0) text += "\x7f";
    corpus.push_back(text);
  }

  std::size_t asciiCases = 0;
  for (const auto& text : corpus) {
    // The width the UTF-16 algorithm assigns: visible units for text below
    // DEL without ESC, otherwise the full segmentation path.
    bool pure = true;
    double visible = 0;
    for (const auto unit : batbox::value::utf16CodeUnits(text)) {
      if (unit >= 127 || unit == 0x1b) pure = false;
      else if (unit > 0x1f) ++visible;
    }
    const auto slowWidth = pure ? visible : width->stringWidthJavaScript(Json(text));
    assert(width->stringWidthUtf8(text) == slowWidth);
    assert(width->stringWidth(Json(text)) == slowWidth);
    const auto fastGlyphs = tf::indigenousGlyphCells(*segmenter, *width, text);
    const auto slowGlyphs = tf::segmentedGlyphCells(*segmenter, *width, text);
    if (fastGlyphs != slowGlyphs) {
      std::cerr << "glyph mismatch for: " << Json(text).dump() << "\n";
      return 1;
    }
    assert(ftxui::Utf8ToGlyphs(text) == slowGlyphs);
    bool ascii = true;
    for (const unsigned char c : text) if (c >= 0x7f) ascii = false;
    if (ascii) ++asciiCases;
    if (text.find('\x1b') == std::string::npos) {
      bool finite = true;
      try { (void)ftxui::string_width(text); } catch (...) { finite = false; }
      assert(finite);
      assert(ftxui::string_width(text) == static_cast<int>(slowWidth));
    }
  }
  assert(asciiCases > 250);
  std::cout << "ASCII width and glyph fast paths match the Unicode path on "
            << corpus.size() << " strings\n";
  return 0;
}
