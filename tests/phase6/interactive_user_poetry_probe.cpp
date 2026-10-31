#include "interactive/cards/UserPoetryLeaf.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <set>
using batbox::Json;
using namespace batbox::interactive::cards;

static std::string fullText(const Json& item) {
  if (!item.contains("length")) return item.value("text", "");
  auto value = item.value("prefix", "");
  const auto length = item.at("length").get<std::size_t>();
  const auto interval = item.value("lineInterval", 0);
  while (batbox::value::utf16CodeUnits(value).size() < length) {
    auto size = batbox::value::utf16CodeUnits(value).size();
    value += interval && size % static_cast<std::size_t>(interval) == 0 ? "\n" : "x";
  }
  return value + item.value("tail", "");
}
int main(int argc, char** argv) {
  if (argc != 3 || std::string(argv[1]) != "--cases") return 2;
  std::ifstream input(argv[2]);
  Json output = Json::array();
  for (const auto& item : Json::parse(input)) {
    std::set<std::string> features;
    for (const auto& flag : item.value("features", Json::array()))
      features.insert(flag.get<std::string>());
    Json errors = Json::array();
    PoetryEnvironment env;
    env.briefFeatureEnabled = features.contains("KAIROS") ||
                              features.contains("KAIROS_BRIEF");
    env.isBriefOnly = item.value("briefOnly", false);
    env.isTranscriptMode = item.value("transcript", false);
    env.viewingBatMission = item.value("viewingAgentTask", false);
    env.briefEnvironmentEnabled = item.value("briefEnvironment", false);
    env.selected = item.value("selected", false);
    env.briefModeActive = [&] { return item.value("active", false); };
    env.userMessageOptIn = [&] { return item.value("optIn", false); };
    env.briefPreference = [&] { return item.value("preference", false); };
    env.logError = [&](const std::string& error) { errors.push_back(error); };
    const auto timestamp = item.contains("timestamp") ?
        std::optional<std::string>{item.at("timestamp").get<std::string>()} :
        std::nullopt;
    const auto leaf = poetryLeaf(fullText(item), timestamp,
                                 item.value("margin", false), env);
    Json value;
    if (leaf) {
      const auto background = leaf->background == PoetryBackground::Selected ?
          Json("messageActionsBackground") :
          leaf->background == PoetryBackground::User ?
          Json("userMessageBackground") : Json(nullptr);
      value = Json{{"text", leaf->displayText}, {"brief", leaf->briefLayout},
                   {"timestamp", leaf->timestamp ? Json(*leaf->timestamp) : Json(nullptr)},
                   {"background", background}, {"margin", leaf->addMargin ? 1 : 0},
                   {"paddingRight", leaf->paddingRight}};
    }
    output.push_back({{"name", item.at("name")}, {"value", value}, {"errors", errors}});
  }
  std::cout << output.dump() << '\n';
}
