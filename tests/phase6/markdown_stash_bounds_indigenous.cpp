#include "interactive/markdown/Renderer.hpp"
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <cassert>
#include <iostream>
#include <string>

using namespace batbox::interactive::markdown;

namespace {
std::size_t tokenCount(const Tokens& tokens) {
  std::size_t count = 0;
  for (const auto& token : tokens) count += 1 + tokenCount(token.children) + tokenCount(token.items);
  return count;
}
std::string draw(const ftxui::Element& element, int columns) {
  auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(columns),
                                      ftxui::Dimension::Fixed(400));
  ftxui::Render(screen, element);
  return screen.ToString();
}
} // namespace

int main() {
  {
    TokenStash stash(1024 * 1024);
    for (int i = 0; i < 200; ++i) {
      std::string content = "# Title " + std::to_string(i) + "\n\n";
      while (content.size() < 2048) content += "- item **" + std::to_string(i) + "** `code`\n";
      const auto& tokens = stash.get(content);
      assert(tokenCount(tokens) == tokenCount(lex(content)));
      if (i == 0) std::cout << "one entry holds " << stash.bytes() << " bytes\n";
      assert(stash.bytes() <= 1024 * 1024);
    }
    assert(stash.size() > 0 && stash.size() < 200);
    std::string huge = "# Huge\n\n";
    while (huge.size() < 1024 * 1024) huge += "| a | b |\n|---|---|\n| 1 | 2 |\n\n";
    const auto before = stash.size();
    assert(tokenCount(stash.get(huge)) == tokenCount(lex(huge)));
    assert(stash.bytes() <= 1024 * 1024);
    assert(stash.size() <= before);
    const auto& plain = stash.get("no markdown syntax here");
    assert(plain.size() == 1 && plain.front().kind == Kind::Paragraph);
    stash.clear();
    assert(stash.size() == 0 && stash.bytes() == 0);
  }
  {
    TokenStash stash;
    std::string content = "**repeat** me";
    const auto* first = &stash.get(content);
    const auto* second = &stash.get(content);
    assert(first == second);
  }

  auto services = indigenousFormatServices(false, false, "", [](const std::string& text) {
    return static_cast<int>(text.size());
  });
  const WrapAnsi wrap = [](const std::string& value, int, bool) { return value; };
  std::string document = "# Streaming\n\nFirst paragraph with **bold** text.\n\n"
      "- one\n- two\n- three\n\n| a | b |\n|---|---|\n| 1 | 2 |\n\n```cpp\n";
  for (int i = 0; i < 30; ++i) document += "int v" + std::to_string(i) + " = " + std::to_string(i) + ";\n";
  document += "```\n\nClosing paragraph after the code block.\n\nTail words";

  Renderer cached(services, wrap, 70);
  StreamingState state;
  int columns = 70;
  std::size_t reused = 0;
  for (std::size_t end = 1; end <= document.size(); end += 7) {
    if (end > document.size() / 2 && columns == 70) {
      columns = 50;
      cached.setTerminalColumns(columns);
    }
    const auto text = document.substr(0, end);
    const auto prefixBefore = state.stablePrefix;
    const auto element = cached.renderStreaming(state, text);
    if (!prefixBefore.empty() && prefixBefore == state.stablePrefix) ++reused;
    Renderer fresh(services, wrap, columns);
    const auto suffix = stripPoetryXMLTags(text).substr(state.stablePrefix.size());
    ftxui::Elements parts;
    if (!state.stablePrefix.empty()) parts.push_back(fresh.render(state.stablePrefix));
    if (!state.stablePrefix.empty() && !suffix.empty()) parts.push_back(ftxui::text(""));
    if (!suffix.empty()) parts.push_back(fresh.render(suffix));
    assert(draw(element, columns) == draw(ftxui::vbox(std::move(parts)), columns));
  }
  assert(reused > 10);
  state.reset();
  assert(state.stablePrefix.empty());

  std::cout << "Markdown token stash is byte bounded and streaming reuses the stable prefix\n";
  return 0;
}
