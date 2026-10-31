#include "interactive/cards/UserTextCard.hpp"
#include <fstream>
#include <iostream>
#include <set>
using batbox::Json;
using namespace batbox::interactive::cards;

int main(int argc, char** argv) {
  if (argc != 3 || std::string(argv[1]) != "--cases") return 2;
  std::ifstream input(argv[2]);
  const auto cases = Json::parse(input);
  Json output = Json::array();
  for (const auto& spec : cases) {
    std::set<std::string> features;
    for (const auto& item : spec.value("features", Json::array()))
      features.insert(item.get<std::string>());
    UserTextProtocol protocol{
      "__NO_CONTENT__", "__INTERRUPT__", "__INTERRUPT_TOOL__",
      "command-message", "local-command-caveat", "tick",
      "teammate-message", "task-notification",
      "KAIROS_GITHUB_WEBHOOKS", "FORK_SUBAGENT", "UDS_INBOX",
      "KAIROS", "KAIROS_CHANNELS",
      [&](const std::string& flag) { return features.contains(flag); },
      [&] { return features.contains("AGENT_SWARMS"); }
    };
    const auto card = classifyUserText(spec.at("text").get<std::string>(),
      spec.value("planContent", ""), protocol);
    output.push_back({{"name", spec.at("name")}, {"kind", userTextKindName(card.kind)}});
  }
  std::cout << output.dump() << '\n';
}
