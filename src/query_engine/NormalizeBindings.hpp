#pragma once
#include "query_engine/NormalizeMessage.hpp"
#include "query_engine/LifecycleServices.hpp"
#include "messages/Messages.hpp"
namespace batbox::query_engine {
bool isNotEmptyMessage(const MessageRef& message);
std::shared_ptr<NormalizeServices> makeNormalizeServices(
  std::shared_ptr<messages::Services> messages,
  std::function<std::string()> flight,
  std::function<std::optional<std::string>(const std::string&)> environment,
  std::function<double()> now);
void bindConversationMessageHelpers(LifecycleServices& lifecycle,
  std::shared_ptr<NormalizeServices> services,std::shared_ptr<NormalizeState> state);
}
