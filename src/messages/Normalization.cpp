#include "messages/Internal.hpp"

namespace batbox::messages {
using namespace internal;
Json normalizeMessages(const Json& messages, Services& services) {
  Json result = Json::array(); bool isNewChain = false;
  for (const auto& message : messages) {
    const std::string type = field(message, "type");
    if (type == "assistant") {
      const Json& content = message.at("message").at("content");
      isNewChain = isNewChain || content.size() > 1;
      for (std::size_t index = 0; index < content.size(); ++index) {
        Json normalized = {{"type", "assistant"}};
        copyIf(normalized, message, "timestamp");
        Json inner = message.at("message"); inner["content"] = Json::array({content[index]});
        if (!has(inner, "context_management") || inner["context_management"].is_null()) inner["context_management"] = nullptr;
        normalized["message"] = std::move(inner);
        for (const auto* key : {"isMeta", "isVirtual", "requestId"}) copyIf(normalized, message, key);
        normalized["uuid"] = isNewChain ? deriveUUID(field(message, "uuid"), index) : field(message, "uuid");
        for (const auto* key : {"error", "isApiErrorMessage", "advisorModel"}) copyIf(normalized, message, key);
        result.push_back(std::move(normalized));
      }
    } else if (type == "attachment" || type == "progress" || type == "system") result.push_back(message);
    else if (type == "user") {
      const Json& content = message.at("message").at("content");
      if (content.is_string()) {
        Json normalized = message;
        normalized["uuid"] = isNewChain ? deriveUUID(field(message, "uuid"), 0) : field(message, "uuid");
        normalized["message"]["content"] = Json::array({textBlock(str(content))});
        result.push_back(std::move(normalized));
      } else {
        isNewChain = isNewChain || content.size() > 1;
        std::size_t imageIndex = 0;
        for (std::size_t index = 0; index < content.size(); ++index) {
          const Json& block = content[index]; bool isImage = isBlock(block, "image");
          Json imageId;
          if (isImage && has(message, "imagePasteIds")) {
            const Json& imageIds = message.at("imagePasteIds");
            if (imageIds.is_array() && imageIndex < imageIds.size()) imageId = imageIds[imageIndex];
          }
          if (isImage) ++imageIndex;
          Json args = {{"content", Json::array({block})}};
          for (const auto* key : {"toolUseResult", "mcpMeta", "isMeta", "isVisibleInTranscriptOnly", "isVirtual", "timestamp", "origin"}) copyIf(args, message, key);
          if (!imageId.is_null()) args["imagePasteIds"] = Json::array({imageId});
          Json normalized = createUserMessage(args, services);
          normalized["uuid"] = isNewChain ? deriveUUID(field(message, "uuid"), index) : field(message, "uuid");
          result.push_back(std::move(normalized));
        }
      }
    } else result.push_back(nullptr); // flatMap includes an undefined result for an unhandled message type
  }
  return result;
}
} // namespace batbox::messages
