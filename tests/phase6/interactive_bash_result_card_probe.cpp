#include "interactive/cards/BashResultCard.hpp"
#include <fstream>
#include <iostream>
using batbox::Json;
using namespace batbox::interactive::cards;

int main(int argc, char** argv) {
  if (argc != 3 || std::string(argv[1]) != "--cases") return 2;
  std::ifstream input(argv[2]);
  Json output = Json::array();
  for (const auto& item : Json::parse(input)) {
    const auto timeout = item.contains("timeoutMs") ?
        std::optional<double>{item.at("timeoutMs").get<double>()} : std::nullopt;
    const auto card = buildBashResultCard(item.at("content"),
                                          item.value("verbose", false), timeout);
    Json value{{"name", item.at("name")}, {"image", card.image}};
    if (!card.image) {
      value["stdout"] = card.stdoutText ? Json(*card.stdoutText) : Json(nullptr);
      value["stderr"] = card.stderrText ? Json(*card.stderrText) : Json(nullptr);
      value["warning"] = card.cwdResetWarning ? Json(*card.cwdResetWarning) : Json(nullptr);
      value["noOutput"] = card.noOutputText ? Json(*card.noOutputText) : Json(nullptr);
      value["backgroundHint"] = card.backgroundHint;
      value["timeoutMs"] = card.timeoutMs ? Json(*card.timeoutMs) : Json(nullptr);
      value["verbose"] = card.verbose;
    }
    output.push_back(std::move(value));
  }
  std::cout << output.dump() << '\n';
}
