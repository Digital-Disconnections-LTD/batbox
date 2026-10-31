#include "interactive/cards_assistant/ThinkingCard.hpp"
#include <fstream>
#include <iostream>

using namespace batbox::interactive::cards_assistant;
using batbox::Json;

int main(int argc, char** argv) {
  if (argc != 3 || std::string(argv[1]) != "--cases") return 2;
  std::ifstream stream(argv[2]);
  Json results = Json::array();
  for (const auto& item : Json::parse(stream)) {
    const bool margin = item.value("margin", false);
    const auto card = item.value("redacted", false) ?
      classifyRedactedThinking(margin) :
      classifyThinking(item.at("text").get<std::string>(), margin,
        item.value("transcript", false), item.value("verbose", false),
        item.value("hide", false));
    const auto kind = card.kind;
    results.push_back({{"name", item.at("name")},
      {"kind", thinkingKindName(kind)},
      {"heading", kind == ThinkingKind::Hidden ? "" :
        kind == ThinkingKind::Redacted ? "✻ Thinking…" :
        kind == ThinkingKind::Expanded ? "∴ Thinking…" : "∴ Thinking "},
      {"content", kind == ThinkingKind::Expanded ? card.thinking : ""},
      {"margin", kind != ThinkingKind::Hidden && card.addMargin},
      {"shortcut", kind == ThinkingKind::Collapsed}});
  }
  std::cout << results.dump() << '\n';
}
