#include "interactive/MessageListPipeline.hpp"
#include "messages/Context.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::interactive {
namespace {
MessageRefs removeProgress(const MessageRefs& messages) {
  MessageRefs result;
  for (const auto& message : messages)
    if (message && message->value("type", "") != "progress") result.push_back(message);
  return result;
}
RenderItemsRef sliceItems(const RenderItemsRef& input, std::size_t begin,
                          std::size_t end) {
  auto result = std::make_shared<RenderItems>();
  begin = std::min(begin, input->size());
  end = std::clamp(end, begin, input->size());
  result->insert(result->end(), input->begin() + begin, input->begin() + end);
  return result;
}
} // namespace

PreparedMessageList prepareMessageList(const MessageRefs& messages,
                                       const MessageListSettings& settings,
                                       std::optional<SliceAnchor>& anchor) {
  if (!settings.gadgets) throw std::logic_error("Grouping Gadgets required");
  if (!settings.messageServices) throw std::logic_error("Message services required");
  PreparedMessageList output;
  const auto normalized = normalizeMessagesRefs(messages, *settings.messageServices);
  for (const auto& message : normalized)
    if (message && isNotEmptyMessage(*message, settings.visibility))
      output.normalizedMessages.push_back(message);
  MessageRefs synthetic;
  if (!settings.streamingGadgetUses.empty()) {
    synthetic = syntheticStreamingGadgetUseMessages(settings.streamingGadgetUses,
        settings.inProgressGadgetUseIDs, output.normalizedMessages,
        *settings.messageServices);
  }
  const bool shouldTruncate = settings.transcriptMode &&
      !settings.showAllInTranscript && !settings.virtualScroll;
  MessageRefs compactAware;
  if (settings.verbose || settings.fullscreen)
    compactAware = output.normalizedMessages;
  else {
    std::size_t start = 0;
    for (std::size_t i = output.normalizedMessages.size(); i > 0; --i)
      if (output.normalizedMessages[i - 1] &&
          messages::isCompactBoundaryMessage(*output.normalizedMessages[i - 1])) {
        start = i - 1;
        break;
      }
    compactAware.assign(output.normalizedMessages.begin() + start,
                        output.normalizedMessages.end());
  }
  auto ordered = reorderMessagesInUI(removeProgress(compactAware), synthetic);
  MessageRefs visible;
  for (const auto& message : ordered)
    if (message && !isNullRenderingAttachment(*message) &&
        shouldShowUserMessage(*message, settings.transcriptMode,
                              settings.visibility)) visible.push_back(message);
  const auto notTruncatedCount = visible.size();
  MessageListRef briefFiltered = std::make_shared<MessageRefs>(std::move(visible));
  if (!settings.briefGadgetNames.empty() && !settings.transcriptMode) {
    if (settings.briefOnly)
      briefFiltered = std::make_shared<MessageRefs>(
          filterForBriefGadget(*briefFiltered, settings.briefGadgetNames));
    else if (!settings.dropTextGadgetNames.empty())
      briefFiltered = dropTextInBriefTurns(briefFiltered, settings.dropTextGadgetNames);
  }
  output.hasTruncatedMessages = shouldTruncate && briefFiltered->size() > 30;
  output.hiddenMessageCount = static_cast<std::ptrdiff_t>(notTruncatedCount) - 30;
  if (shouldTruncate && briefFiltered->size() > 30)
    output.messagesToShow.assign(briefFiltered->end() - 30, briefFiltered->end());
  else output.messagesToShow = *briefFiltered;

  auto grouped = applyGrouping(asRenderItems(output.messagesToShow),
                               settings.gadgets, settings.verbose);
  auto collapsed = collapseReadSearchGroups(grouped, settings.readSearch);
  collapsed = collapsePartnerShutdowns(collapsed);
  collapsed = collapseHookSummaries(collapsed);
  collapsed = collapseBackgroundBashNotifications(collapsed,
      settings.verbose, settings.fullscreen, settings.backgroundProtocol);
  output.collapsed = collapsed;
  output.lookups = buildMessageLookups(output.normalizedMessages,
                                       output.messagesToShow);

  const bool capApplies = !settings.virtualScroll && !settings.disableRenderCap;
  MessageRefs collapsedRefs;
  collapsedRefs.reserve(collapsed->size());
  for (const auto& item : *collapsed) collapsedRefs.push_back(item.message);
  output.sliceStart = capApplies ? computeSliceStart(collapsedRefs, anchor) : 0;
  if (settings.renderRange)
    output.renderable = sliceItems(collapsed, settings.renderRange->first,
                                 settings.renderRange->second);
  else if (output.sliceStart > 0)
    output.renderable = sliceItems(collapsed,
        static_cast<std::size_t>(output.sliceStart), collapsed->size());
  else output.renderable = collapsed;
  return output;
}
} // namespace batbox::interactive
