#include "interactive/MessageOrder.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using batbox::Json;
namespace ix = batbox::interactive;

int main(int argc, char** argv) {
  if (argc != 3 || std::string(argv[1]) != "--cases")
    throw std::invalid_argument("Usage: probe --cases path");
  std::ifstream input(argv[2]);
  const auto cases = Json::parse(input);
  Json rows = Json::array();
  for (const auto& spec : cases) {
    ix::MessageRefs messages, synthetic;
    for (const auto& message : spec.at("messages"))
      messages.push_back(std::make_shared<Json>(message));
    if (spec.contains("synthetic"))
      for (const auto& message : spec.at("synthetic"))
        synthetic.push_back(std::make_shared<Json>(message));
    Json output = Json::array();
    for (const auto& message : ix::reorderMessagesInUI(messages, synthetic))
      output.push_back(message->at("id"));
    rows.push_back(Json{{"name", spec.at("name")}, {"output", output}});
  }
  std::cout << rows.dump() << '\n';
}
