#pragma once
#include "query_engine/Types.hpp"
#include <list>
namespace batbox::query_engine {
struct NormalizeServices {
  std::function<MessagesRef(const MessagesRef&)> normalizeMessages;
  std::function<bool(const MessageRef&)> isNotEmptyMessage;
  std::function<std::string()> getFlightId;
  std::function<std::optional<std::string>(const std::string&)> env;
  std::function<double()> now;
};
struct NormalizeState {
  std::list<std::pair<std::string,double>> gadgetProgressLastSentTime;
};
SdkCursor normalizeMessage(MessageRef message,std::shared_ptr<NormalizeState> state,
  std::shared_ptr<NormalizeServices> services);
}
