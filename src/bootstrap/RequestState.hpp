#pragma once
#include "runtime/JsonValue.hpp"
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace batbox::bootstrap {
struct ErrorLogEntry {
  std::string error;
  std::string timestamp;
};
struct RequestState {
  std::shared_ptr<Json> lastAPIRequest;
  std::shared_ptr<Json> lastAPIRequestMessages;
  std::shared_ptr<Json> lastClassifierRequests;
  std::optional<std::string> stashedBatboxMdContent;
  std::vector<ErrorLogEntry> inMemoryErrorLog;
};
void setLastAPIRequest(RequestState& state,std::shared_ptr<Json> params);
std::shared_ptr<Json> getLastAPIRequest(const RequestState& state);
void setLastAPIRequestMessages(RequestState& state,
  std::shared_ptr<Json> messages);
std::shared_ptr<Json> getLastAPIRequestMessages(const RequestState& state);
void setLastClassifierRequests(RequestState& state,
  std::shared_ptr<Json> requests);
std::shared_ptr<Json> getLastClassifierRequests(const RequestState& state);
void setStashedBatboxMdContent(RequestState& state,
  std::optional<std::string> content);
const std::optional<std::string>& getStashedBatboxMdContent(
  const RequestState& state);
void addToInMemoryErrorLog(RequestState& state,ErrorLogEntry errorInfo);
}
