#pragma once

#include "interactive/MessageListPipeline.hpp"
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace batbox::interactive {

// Fullscreen frames prepare the message list many times per second while the
// list itself rarely changes. The memo keeps the prepared list between frames
// and normalizes only messages that are new, replaced, or edited in place.
// In-place edits are discovered through runtime::noteMessageMutated.
class MessageListMemo {
public:
  std::shared_ptr<PreparedMessageList> prepare(const MessageRefs& messages,
      const MessageListSettings& settings, std::optional<SliceAnchor>& anchor);
  void clear();
  // Messages normalized since construction; lets tests observe cache reuse.
  std::uint64_t normalizations() const { return normalizations_; }
  // Full list preparations since construction (memo misses).
  std::uint64_t preparations() const { return preparations_; }

private:
  struct Normalized {
    std::weak_ptr<Json> source;
    bool chainIn = false;
    bool chainOut = false;
    MessageRefs output;
  };
  struct Key {
    bool transcriptMode = false, showAllInTranscript = false,
         virtualScroll = false, disableRenderCap = false, verbose = false,
         briefOnly = false, fullscreen = false;
    std::optional<std::pair<std::size_t, std::size_t>> renderRange;
    std::vector<std::string> briefGadgetNames, dropTextGadgetNames;
    std::vector<std::pair<std::string, bool>> groupingGadgets;
    gadgets::Gadgets readSearchGadgets;
    std::string noContentMessage, interruptMessageForGadgetUse;
    BackgroundNotificationProtocol backgroundProtocol;
    std::vector<StreamingGadgetUse> streamingGadgetUses;
    GadgetIDs inProgressGadgetUseIDs;
    const messages::Services* messageServices = nullptr;
  };
  static bool matches(const Key& key, const MessageListSettings& settings);
  static Key keyFor(const MessageListSettings& settings);

  std::unordered_map<const Json*, Normalized> normalized_;
  std::uint64_t epoch_ = 0;
  bool epochKnown_ = false;
  MessageRefs lastMessages_;
  std::optional<Key> lastKey_;
  std::shared_ptr<PreparedMessageList> result_;
  std::uint64_t normalizations_ = 0;
  std::uint64_t preparations_ = 0;
};

} // namespace batbox::interactive
