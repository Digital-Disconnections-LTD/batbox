#pragma once

#include "interactive/MessageCollapse.hpp"
#include "interactive/MessageLookups.hpp"
#include "interactive/MessageNormalizationRefs.hpp"
#include "interactive/MessageSynthetic.hpp"
#include "interactive/ReadSearchCollapse.hpp"
#include "interactive/MessageVisibility.hpp"
#include <functional>
#include <optional>
#include <utility>

namespace batbox::interactive {

struct MessageListSettings {
  bool transcriptMode = false;
  bool showAllInTranscript = false;
  bool virtualScroll = false;
  bool disableRenderCap = false;
  bool verbose = false;
  bool briefOnly = false;
  bool fullscreen = false;
  std::optional<std::pair<std::size_t, std::size_t>> renderRange;
  std::vector<std::string> briefGadgetNames;
  std::vector<std::string> dropTextGadgetNames;
  GroupingGadgets gadgets;
  BackgroundNotificationProtocol backgroundProtocol;
  VisibilityImports visibility;
  std::vector<StreamingGadgetUse> streamingGadgetUses;
  GadgetIDs inProgressGadgetUseIDs;
  messages::Services* messageServices = nullptr;
  ReadSearchImports readSearch;
};

struct PreparedMessageList {
  MessageRefs normalizedMessages;
  MessageRefs messagesToShow;
  RenderItemsRef collapsed;
  RenderItemsRef renderable;
  MessageLookups lookups;
  bool hasTruncatedMessages = false;
  std::ptrdiff_t hiddenMessageCount = 0;
  double sliceStart = 0;
};

PreparedMessageList prepareMessageList(const MessageRefs& messages,
                                       const MessageListSettings& settings,
                                       std::optional<SliceAnchor>& anchor);
// prepareMessageList after normalizeMessagesRefs has already run.
PreparedMessageList prepareNormalizedMessageList(MessageRefs normalized,
                                                 const MessageListSettings& settings,
                                                 std::optional<SliceAnchor>& anchor);

} // namespace batbox::interactive
