#include "bats/ForkedBat.hpp"
#include <cassert>
#include <iostream>

using batbox::Json;
namespace bt = batbox::bats;

int main() {
  Json conversation = Json::array();
  for (int i = 0; i < 40; ++i)
    conversation.push_back(Json{{"type", i % 2 ? "assistant" : "user"},
        {"uuid", "m" + std::to_string(i)}, {"message", {{"content", std::string(2000, 'x')}}}});

  auto full = std::make_shared<bt::BatContext>();
  full->query.data = Json{{"agentId", nullptr}, {"options", {{"mainLoopModel", "model"}}}};
  batbox::execution::Context current = full->query;
  current.data["messages"] = conversation;
  current.data["queryTracking"] = Json{{"chainId", "chain"}, {"depth", 0}};

  const auto snapshot = bt::makeStashSafeSnapshot(full, Json::array({"system"}),
      Json{{"user", 1}}, Json{{"system", 2}}, current, conversation);
  assert(snapshot);
  // The conversation is kept once, as the fork's starting messages.
  assert(snapshot->forkContextMessages == conversation);
  assert(snapshot->gadgetUseContext.query.data.at("messages") == Json::array());
  // Every other part of the live query context is preserved.
  assert(snapshot->gadgetUseContext.query.data.at("queryTracking") ==
         current.data.at("queryTracking"));
  assert(snapshot->gadgetUseContext.query.data.at("options") == current.data.at("options"));
  assert(snapshot->systemPoetry == Json::array({"system"}));
  assert(snapshot->userContext == Json({{"user", 1}}));
  assert(snapshot->systemContext == Json({{"system", 2}}));
  // The caller's context is not modified.
  assert(current.data.at("messages") == conversation);

  // Contexts without a messages array pass through unchanged.
  batbox::execution::Context bare = full->query;
  const auto plain = bt::makeStashSafeSnapshot(full, Json::array(), Json::object(),
      Json::object(), bare, Json::array());
  assert(!plain->gadgetUseContext.query.data.contains("messages"));
  assert(plain->gadgetUseContext.query.data == bare.data);

  std::cout << "stash-safe snapshots keep one copy of the conversation\n";
  return 0;
}
