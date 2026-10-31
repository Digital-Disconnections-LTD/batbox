#include "interactive/cards/ThinkingHighlight.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
using batbox::Json;
using namespace batbox::interactive::cards;
static Json parts(const std::vector<HighlightPart>& input) {
  Json result = Json::array();
  for (const auto& part : input)
    result.push_back({{"text", part.text}, {"color", part.color}});
  return result;
}
int main(int argc, char** argv) {
  if (argc != 3 || std::string(argv[1]) != "--cases") return 2;
  std::ifstream input(argv[2]);
  Json output = Json::array();
  for (const auto& item : Json::parse(input)) {
    HighlightOptions options;
    options.queued = item.value("queued", false);
    options.selected = item.value("selected", false);
    options.ultrathinkEnabled = item.value("ultrathink", false);
    options.pointerGlyph = "❯";
    options.formatBriefTimestamp = [](const std::string& value) {
      return "T(" + value + ")";
    };
    const auto timestamp = item.contains("timestamp") ?
        std::optional<std::string>{item.at("timestamp").get<std::string>()} :
        std::nullopt;
    const auto view = highlightedThinkingText(item.at("text").get<std::string>(),
        item.value("brief", false), timestamp, options);
    output.push_back({{"name", item.at("name")}, {"brief", view.brief},
      {"heading", parts(view.heading)}, {"content", parts(view.content)}});
  }
  std::cout << output.dump() << '\n';
}
