#pragma once

#include "interactive/MessageOrder.hpp"
#include <functional>
#include <memory>
#include <optional>

namespace batbox::interactive {

struct VisibilityImports {
  std::string noContentMessage;
  std::string interruptMessageForGadgetUse;
};
bool isNotEmptyMessage(const Json& message, const VisibilityImports& imports);
bool shouldShowUserMessage(const Json& message, bool transcriptMode,
                           const VisibilityImports& imports);
bool isNullRenderingAttachment(const Json& message);

using MessageListRef = std::shared_ptr<MessageRefs>;

struct SliceAnchor {
  std::string uuid;
  double idx = 0;
};
double computeSliceStart(const MessageRefs& collapsed,
                         std::optional<SliceAnchor>& anchor,
                         double cap = 200, double step = 50);

} // namespace batbox::interactive
