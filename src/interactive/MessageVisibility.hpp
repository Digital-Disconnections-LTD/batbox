#pragma once

#include "interactive/MessageOrder.hpp"
#include <functional>
#include <memory>
#include <optional>

namespace batbox::interactive {

struct VisibilityImports {
  std::string noContentMessage;
  std::string interruptMessageForGadgetUse;
  std::function<bool(const std::string&)> feature;
};
bool isNotEmptyMessage(const Json& message, const VisibilityImports& imports);
bool shouldShowUserMessage(const Json& message, bool transcriptMode,
                           const VisibilityImports& imports);
bool isNullRenderingAttachment(const Json& message);
MessageRefs filterForBriefGadget(const MessageRefs& messages,
                                const std::vector<std::string>& briefGadgetNames);

using MessageListRef = std::shared_ptr<MessageRefs>;
MessageListRef dropTextInBriefTurns(const MessageListRef& messages,
                                    const std::vector<std::string>& briefGadgetNames);

struct SliceAnchor {
  std::string uuid;
  double idx = 0;
};
double computeSliceStart(const MessageRefs& collapsed,
                         std::optional<SliceAnchor>& anchor,
                         double cap = 200, double step = 50);

} // namespace batbox::interactive
