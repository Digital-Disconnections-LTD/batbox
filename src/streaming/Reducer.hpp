#pragma once

#include "runtime/JsonValue.hpp"
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace batbox::streaming {

enum class StreamErrorKind { MissingBlock, MissingMessage, TypeMismatch, InvalidGadgetInput, IncompleteStream };
class StreamError : public std::runtime_error {
 public:
  StreamError(StreamErrorKind kind, const std::string& message)
    : std::runtime_error(message), kind_(kind) {}
  StreamErrorKind kind() const { return kind_; }
  const char* name() const { return kind_ == StreamErrorKind::MissingBlock ? "RangeError" : "Error"; }
 private:
  StreamErrorKind kind_;
};

// The stream loop yields shared message objects. Retain these pointers:
// message_delta mutates the last assistant message after it has been yielded.
using MessageRef = std::shared_ptr<Json>;
using Emissions = std::vector<MessageRef>;

struct ReducerConfig {
  std::string model;
  std::string resolvedModel;
  std::string requestId;
  std::string advisorModel;
  int maxOutputTokens = 0;
  std::int64_t startMillis = 0;
  bool connectorTextEnabled = false;
  bool stashedMicrocompact = false;
  std::function<std::int64_t()> nowMillis;
  std::function<std::string()> randomUUID;
  std::function<std::string()> timestamp;
  // Injected reducer dependencies. The normalizer can use
  // normalizeContentFromAPI below, with gadget-specific hooks from the caller.
  std::function<Json(const Json&)> normalizeContent;
  std::function<double(const std::string&, const Json&)> calculateUSDCost;
  std::function<double(double, const Json&, const std::string&)> addToTotalFlightCost;
  std::function<std::optional<Json>(const Json&, const std::string&)> getErrorMessageIfRefusal;
  std::function<Json(const std::string&, const std::string&, const std::string&)> createAssistantAPIErrorMessage;
};

struct NormalizeGadgets {
  std::function<std::optional<Json>(const std::string&)> findGadgetByName;
  std::function<Json(const Json&, const Json&, const std::string&)> normalizeGadgetInput;
  std::string batId;
};

Json normalizeContentFromAPI(const Json& contentBlocks, const NormalizeGadgets& gadgets = {});

class Reducer {
 public:
  explicit Reducer(ReducerConfig config);
  Emissions consume(const Json& part);
  // Generator suspension points within one SSE event. beginEvent
  // yields the first value; each nextEmission resumes at the next
  // statement, so callbacks after a refusal yield run only on demand.
  std::optional<MessageRef> beginEvent(const Json& part);
  std::optional<MessageRef> nextEmission();
  void finish() const;
  const std::vector<MessageRef>& messages() const { return newMessages_; }
  const Json& usage() const { return usage_; }
  const Json& stopReason() const { return stopReason_; }
  bool hasMessageStart() const { return value::truthy(partialMessage_); }
  double costUSD() const { return costUSD_; }
  std::int64_t ttftMs() const { return ttftMs_; }
  bool advisorInProgress() const { return isAdvisorInProgress_; }
  const std::vector<std::optional<Json>>& contentBlocks() const { return contentBlocks_; }

 private:
  Emissions consumeUnphased(const Json& part);
  struct PendingEvent {
    enum class Phase {Buffered,AfterRefusal,AfterMaxTokens,AfterContextError,RawDone};
    Json part;
    Phase phase=Phase::Buffered;
    Emissions buffered;
    std::size_t index=0;
  };
  std::optional<PendingEvent> pendingEvent_;
  ReducerConfig config_;
  std::vector<MessageRef> newMessages_;
  std::vector<std::optional<Json>> contentBlocks_;
  Json partialMessage_ = nullptr;
  Json usage_;
  Json stopReason_ = nullptr;
  double costUSD_ = 0;
  std::int64_t ttftMs_ = 0;
  bool isAdvisorInProgress_ = false;
};

} // namespace batbox::streaming
