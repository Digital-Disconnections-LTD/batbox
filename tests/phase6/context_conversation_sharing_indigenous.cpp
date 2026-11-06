#include "execution/GadgetExecution.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using batbox::Json;
namespace {
void check(bool condition, const std::string& what) {
  if (!condition) throw std::runtime_error("check failed: " + what);
}

void conversationIsShared() {
  batbox::execution::Context context;
  context.data = Json{{"options", {{"mainLoopModel", "local"}}}, {"messages", Json::array({"stale"})}};
  Json conversation = Json::array();
  for (int i = 0; i < 50; ++i) conversation.push_back(Json{{"type", "user"}, {"text", std::string(1000, 'x')}});
  batbox::execution::shareConversation(context, conversation);
  check(!context.data.contains("messages"), "shared conversation replaces data[messages]");
  check(batbox::execution::conversationMessages(context) == conversation, "conversation readable through the context");
  std::vector<batbox::execution::Context> copies(20, context);
  for (const auto& copy : copies)
    check(&batbox::execution::conversationMessages(copy) == &batbox::execution::conversationMessages(context),
          "copies of the context share one conversation");
  auto overridden = context;
  overridden.data["messages"] = Json::array();
  check(batbox::execution::conversationMessages(overridden).empty(), "data[messages] takes precedence");
  overridden.data["messages"] = nullptr;
  check(batbox::execution::conversationMessages(overridden).size() == conversation.size(), "a null data[messages] falls back");
  check(batbox::execution::conversationMessages(batbox::execution::contextWithoutConversation(context)).is_null(),
        "contextWithoutConversation drops the conversation");
}

} // namespace

int main() {
  conversationIsShared();
  std::cout << "context conversation: copies share one conversation and data[messages] takes precedence\n";
  return 0;
}
