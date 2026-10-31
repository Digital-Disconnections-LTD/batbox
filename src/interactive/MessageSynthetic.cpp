#include "interactive/MessageSynthetic.hpp"
#include "interactive/MessageDecisions.hpp"

namespace batbox::interactive {
MessageRefs syntheticStreamingGadgetUseMessages(
    const std::vector<StreamingGadgetUse>& streamingGadgetUses,
    const GadgetIDs& inProgressGadgetUseIDs,
    const MessageRefs& normalizedMessages,
    messages::Services& messageServices) {
  const auto normalizedIDs = getGadgetUseIDs(normalizedMessages);
  MessageRefs output;
  for (const auto& streaming : streamingGadgetUses) {
    const auto id = streaming.contentBlock.at("id").get<std::string>();
    if (inProgressGadgetUseIDs.contains(id) || normalizedIDs.contains(id)) continue;
    Json assistant = messages::createAssistantMessage(
        Json{{"content", Json::array({streaming.contentBlock})}}, messageServices);
    assistant["uuid"] = messages::deriveUUID(id, 0);
    const auto normalized = messages::normalizeMessages(Json::array({assistant}), messageServices);
    for (const auto& message : normalized)
      output.push_back(std::make_shared<Json>(message));
  }
  return output;
}
} // namespace batbox::interactive
