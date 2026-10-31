#include "interactive/cards_assistant/AssistantTextCard.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
using batbox::Json;
using namespace batbox::interactive::cards_assistant;

int main(int argc, char** argv) {
  if (argc != 3 || std::string(argv[1]) != "--cases") return 2;
  std::ifstream input(argv[2]);
  Json output = Json::array();
  for (const auto& item : Json::parse(input)) {
    Protocol protocol{
      "__NO_CONTENT__", "__NO_RESPONSE__", "Prompt is too long",
      "__CREDIT__", "Credit balance too low · Add funds: https://platform.claude.com/settings/billing",
      "__INVALID__", "__EXTERNAL__", "__ORG__", "__ORG_AUTH__",
      "__REVOKED__", "__TIMEOUT__", "__MODEL_OFF__", "__ABORT__",
      "API Error", {"You've hit your", "You've used",
        "You're now using extra usage", "You're close to",
        "You're out of extra usage"}
    };
    Imports imports;
    imports.contextUpgradeHint = [] {return "";};
    imports.isKeychainLocked = [] {return false;};
    imports.fallbackModelName = [] {return "Fallback";};
    imports.startsWithApiErrorPrefix = [](const std::string& text) {
      return text.starts_with("API Error") ||
        text.starts_with("Please run /login · API Error");
    };
    imports.apiTimeoutEnvironment = item.value("timeoutEnvironment", "");
    const auto text = item.contains("text") ?
        item.at("text").get<std::string>() :
        item.at("textPrefix").get<std::string>() +
        std::string(item.at("count").get<std::size_t>(),
                    item.at("repeat").get<std::string>()[0]);
    const auto card = classifyAssistantText(text, false, false, false,
                                             false, protocol, imports);
    Json display = (card.kind == Kind::Hidden ||
                    card.kind == Kind::Interrupted ||
                    card.kind == Kind::ModelUnavailable) ? Json(nullptr) :
                    Json(card.displayText);
    output.push_back({{"name", item.at("name")},
      {"kind", kindName(card.kind)}, {"display", display},
      {"truncated", card.truncated}});
  }
  std::cout << output.dump() << '\n';
}
