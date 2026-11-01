#pragma once

#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace batbox::interactive {

struct StreamingGadgetUse {
  Json index;
  Json contentBlock;
  std::string unparsedGadgetInput;
};
struct StreamingThinking {
  std::string thinking;
  bool isStreaming = false;
  std::optional<double> streamingEndedAt;
};
using GadgetUsesUpdate = std::function<std::vector<StreamingGadgetUse>(
    const std::vector<StreamingGadgetUse>&)>;
using ThinkingUpdate = std::function<std::optional<StreamingThinking>(
    const std::optional<StreamingThinking>&)>;
using TextUpdate = std::function<std::optional<std::string>(
    const std::optional<std::string>&)>;

struct StreamImports {
  // Imported feature and protocol predicate. Both are evaluated only at the
  // branch where each service is needed.
  std::function<bool()> connectorTextEnabled;
  std::function<bool(const Json&)> isConnectorTextBlock;
  std::function<double()> nowMs;
};
struct StreamCallbacks {
  std::function<void(const Json&)> onMessage;
  std::function<void(const std::string&)> onUpdateLength;
  std::function<void(const std::string&)> onSetStreamMode;
  std::function<void(GadgetUsesUpdate)> onStreamingGadgetUses;
  std::function<void(const Json&)> onTombstone;
  std::function<void(ThinkingUpdate)> onStreamingThinking;
  std::function<void(double)> onApiMetrics;
  std::function<void(TextUpdate)> onStreamingText;
  // Optional in-place accumulators. When bound, deltas are appended to the
  // live buffers directly instead of rebuilding them through the updaters.
  std::function<void(const std::string&)> appendStreamingText;
  std::function<void(const Json& index, const std::string& fragment)> appendGadgetInput;
};

void handleMessageFromStream(const Json& message,
    const StreamCallbacks& callbacks, const StreamImports& imports);

} // namespace batbox::interactive
