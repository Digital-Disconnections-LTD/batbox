#include "interactive/cards_system/SystemAPIErrorCard.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>

using namespace batbox::interactive::cards_system;
using batbox::Json;

int main(int argc, char** argv) {
  if (argc != 3 || std::string(argv[1]) != "--cases") return 2;
  std::ifstream stream(argv[2]);
  Json result = Json::array();
  for (const auto& item : Json::parse(stream)) {
    APIErrorState state;
    const auto error = item.contains("error") ?
      item.at("error").get<std::string>() :
      item.at("errorPrefix").get<std::string>() +
        std::string(item.at("count").get<std::size_t>(),
                    item.at("repeat").get<std::string>().at(0));
    const APIErrorInput input{item.at("attempt").get<double>(),
      item.at("retryMs").get<double>(),item.at("max").get<double>(),
      error,item.value("verbose",false),
      item.contains("timeout") ?
        std::optional<std::string>{item.at("timeout").get<std::string>()} :
        std::nullopt};
    for (int tick = 0; tick <= item.at("ticks").get<int>(); ++tick) {
      const auto card = state.render(input);
      result.push_back({{"name",item.at("name")},{"tick",tick},
        {"visible",card.visible},{"errorText",card.errorText},
        {"retryText",card.retryText},{"truncated",card.truncated},
        {"intervalActive",card.intervalActive}});
      if (card.intervalActive) state.intervalTick(card);
    }
  }
  std::cout << result.dump() << '\n';
}
