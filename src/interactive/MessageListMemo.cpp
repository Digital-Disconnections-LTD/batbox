#include "interactive/MessageListMemo.hpp"
#include "runtime/MessageMutation.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::interactive {
namespace {
bool sameOwner(const std::weak_ptr<Json>& cached, const MessageRef& message) {
  return !cached.owner_before(message) && !message.owner_before(cached);
}
bool sameProtocol(const BackgroundNotificationProtocol& left,
                  const BackgroundNotificationProtocol& right) {
  return left.missionTag == right.missionTag && left.statusTag == right.statusTag &&
      left.summaryTag == right.summaryTag &&
      left.bashSummaryPrefix == right.bashSummaryPrefix;
}
// The list shows a streaming Gadget use from its content block alone, so
// input fragments arriving for it do not change the prepared list.
bool sameStreamingUses(const std::vector<StreamingGadgetUse>& left,
                       const std::vector<StreamingGadgetUse>& right) {
  if (left.size() != right.size()) return false;
  for (std::size_t i = 0; i < left.size(); ++i) {
    if (left[i].index != right[i].index ||
        left[i].contentBlock != right[i].contentBlock) return false;
  }
  return true;
}
bool sameGrouping(const std::vector<std::pair<std::string, bool>>& cached,
                  const GroupingGadgets& gadgets) {
  if (!gadgets) return cached.empty();
  if (cached.size() != gadgets->gadgets.size()) return false;
  for (std::size_t i = 0; i < cached.size(); ++i)
    if (cached[i].first != gadgets->gadgets[i].name ||
        cached[i].second != gadgets->gadgets[i].renderGroupedGadgetUse) return false;
  return true;
}
} // namespace

MessageListMemo::Key MessageListMemo::keyFor(const MessageListSettings& settings) {
  Key key;
  key.transcriptMode = settings.transcriptMode;
  key.showAllInTranscript = settings.showAllInTranscript;
  key.virtualScroll = settings.virtualScroll;
  key.disableRenderCap = settings.disableRenderCap;
  key.verbose = settings.verbose;
  key.briefOnly = settings.briefOnly;
  key.fullscreen = settings.fullscreen;
  key.renderRange = settings.renderRange;
  key.briefGadgetNames = settings.briefGadgetNames;
  key.dropTextGadgetNames = settings.dropTextGadgetNames;
  if (settings.gadgets)
    for (const auto& gadget : settings.gadgets->gadgets)
      key.groupingGadgets.emplace_back(gadget.name, gadget.renderGroupedGadgetUse);
  key.readSearchGadgets = settings.readSearch.gadgets;
  key.noContentMessage = settings.visibility.noContentMessage;
  key.interruptMessageForGadgetUse = settings.visibility.interruptMessageForGadgetUse;
  key.backgroundProtocol = settings.backgroundProtocol;
  for (const auto& use : settings.streamingGadgetUses)
    key.streamingGadgetUses.push_back({use.index, use.contentBlock, {}});
  key.inProgressGadgetUseIDs = settings.inProgressGadgetUseIDs;
  key.messageServices = settings.messageServices;
  return key;
}

bool MessageListMemo::matches(const Key& key, const MessageListSettings& settings) {
  return key.transcriptMode == settings.transcriptMode &&
      key.showAllInTranscript == settings.showAllInTranscript &&
      key.virtualScroll == settings.virtualScroll &&
      key.disableRenderCap == settings.disableRenderCap &&
      key.verbose == settings.verbose && key.briefOnly == settings.briefOnly &&
      key.fullscreen == settings.fullscreen &&
      key.renderRange == settings.renderRange &&
      key.messageServices == settings.messageServices &&
      key.briefGadgetNames == settings.briefGadgetNames &&
      key.dropTextGadgetNames == settings.dropTextGadgetNames &&
      sameGrouping(key.groupingGadgets, settings.gadgets) &&
      key.readSearchGadgets == settings.readSearch.gadgets &&
      key.noContentMessage == settings.visibility.noContentMessage &&
      key.interruptMessageForGadgetUse == settings.visibility.interruptMessageForGadgetUse &&
      sameProtocol(key.backgroundProtocol, settings.backgroundProtocol) &&
      key.inProgressGadgetUseIDs == settings.inProgressGadgetUseIDs &&
      sameStreamingUses(key.streamingGadgetUses, settings.streamingGadgetUses);
}

void MessageListMemo::clear() {
  normalized_.clear();
  epochKnown_ = false;
  lastMessages_.clear();
  lastKey_.reset();
  result_.reset();
}

std::shared_ptr<PreparedMessageList> MessageListMemo::prepare(
    const MessageRefs& messages, const MessageListSettings& settings,
    std::optional<SliceAnchor>& anchor) {
  if (!settings.gadgets) throw std::logic_error("Grouping Gadgets required");
  if (!settings.messageServices) throw std::logic_error("Message services required");
  bool mutated = !epochKnown_;
  const auto mutations = runtime::messagesMutatedSince(epoch_);
  if (!epochKnown_ || !mutations.changed) {
    normalized_.clear();
    mutated = true;
  } else if (!mutations.changed->empty()) {
    for (const auto* changed : *mutations.changed) normalized_.erase(changed);
    mutated = true;
  }
  epoch_ = mutations.epoch;
  epochKnown_ = true;

  // The render cap keeps a slice anchor across calls; only uncapped lists are
  // a pure function of their inputs and can be reused as a whole.
  const bool capApplies = !settings.virtualScroll && !settings.disableRenderCap;
  if (result_ && !capApplies && !mutated && lastKey_ &&
      lastMessages_.size() == messages.size() &&
      std::equal(lastMessages_.begin(), lastMessages_.end(), messages.begin()) &&
      matches(*lastKey_, settings))
    return result_;

  MessageRefs normalized;
  normalized.reserve(messages.size());
  std::unordered_map<const Json*, Normalized> retained;
  retained.reserve(messages.size());
  bool chain = false;
  for (const auto& message : messages) {
    if (!message) throw std::invalid_argument("Message reference required");
    const Normalized* entry = nullptr;
    Normalized fresh;
    if (const auto kept = retained.find(message.get());
        kept != retained.end() && sameOwner(kept->second.source, message) &&
        kept->second.chainIn == chain) {
      entry = &kept->second;
    } else if (const auto old = normalized_.find(message.get());
               old != normalized_.end() && sameOwner(old->second.source, message) &&
               old->second.chainIn == chain &&
               !retained.contains(message.get())) {
      entry = &retained.emplace(message.get(), std::move(old->second)).first->second;
      normalized_.erase(old);
    } else {
      fresh.source = message;
      fresh.chainIn = chain;
      bool next = chain;
      fresh.output = normalizeMessageRef(message, *settings.messageServices, next);
      fresh.chainOut = next;
      ++normalizations_;
      if (!retained.contains(message.get()))
        entry = &retained.emplace(message.get(), std::move(fresh)).first->second;
      else entry = &fresh;
    }
    normalized.insert(normalized.end(), entry->output.begin(), entry->output.end());
    chain = entry->chainOut;
  }
  normalized_ = std::move(retained);

  result_ = std::make_shared<PreparedMessageList>(
      prepareNormalizedMessageList(std::move(normalized), settings, anchor));
  ++preparations_;
  lastMessages_ = messages;
  lastKey_ = keyFor(settings);
  return result_;
}
} // namespace batbox::interactive
