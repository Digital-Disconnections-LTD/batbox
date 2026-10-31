#include "interactive/MessageVisibility.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
using batbox::Json;
namespace ix = batbox::interactive;
namespace {
ix::MessageRefs refs(const Json& messages) {
  ix::MessageRefs result;
  for (const auto& message : messages) result.push_back(std::make_shared<Json>(message));
  return result;
}
Json ids(const ix::MessageRefs& messages) {
  Json result = Json::array();
  for (const auto& message : messages) result.push_back(message->at("id"));
  return result;
}
} // namespace
int main(int argc, char** argv) {
  if (argc != 3 || std::string(argv[1]) != "--cases")
    throw std::invalid_argument("Usage: probe --cases path");
  std::ifstream input(argv[2]);
  const auto cases = Json::parse(input);
  Json result = Json::array();
  for (const auto& spec : cases.at("brief")) {
    std::vector<std::string> names = spec.at("names");
    result.push_back(Json{{"name", spec.at("name")},
      {"output", ids(ix::filterForBriefGadget(refs(spec.at("messages")), names))}});
  }
  for (const auto& spec : cases.at("drop")) {
    auto messages = std::make_shared<ix::MessageRefs>(refs(spec.at("messages")));
    std::vector<std::string> names = spec.at("names");
    auto output = ix::dropTextInBriefTurns(messages, names);
    result.push_back(Json{{"name", spec.at("name")}, {"output", ids(*output)},
      {"same", output == messages}});
  }
  for (const auto& spec : cases.at("slice")) {
    ix::MessageRefs messages;
    for (const auto& uuid : spec.at("uuids"))
      messages.push_back(std::make_shared<Json>(Json{{"uuid", uuid}}));
    std::optional<ix::SliceAnchor> anchor;
    if (!spec.at("anchor").is_null()) anchor = ix::SliceAnchor{
      spec.at("anchor").at("uuid"), spec.at("anchor").at("idx")};
    const auto start = ix::computeSliceStart(messages, anchor, spec.at("cap"), spec.at("step"));
    Json nextAnchor = anchor ? Json{{"uuid", anchor->uuid}, {"idx", anchor->idx}} : Json(nullptr);
    result.push_back(Json{{"name", spec.at("name")}, {"start", start}, {"anchor", nextAnchor}});
  }
  for (const auto& spec : cases.at("leaves")) {
    const auto& message = spec.at("message");
    const Json features = spec.value("features", Json::object());
    ix::VisibilityImports imports{"__NO_CONTENT__", "__INTERRUPT__",
      [features](const std::string& name) { return features.value(name, false); }};
    result.push_back(Json{{"name", spec.at("name")},
      {"nonempty", ix::isNotEmptyMessage(message, imports)},
      {"show", ix::shouldShowUserMessage(message, spec.value("transcript", false), imports)},
      {"nullAttachment", ix::isNullRenderingAttachment(message)}});
  }
  std::cout << result.dump() << '\n';
}
