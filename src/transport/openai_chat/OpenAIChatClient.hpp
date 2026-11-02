#pragma once
#include "transport/RawHttp.hpp"
#include "transport/WireClient.hpp"

namespace batbox::transport::openai_chat {

// OpenAI Chat Completions servers behind the Anthropic-shaped seam:
// config.messagesPath is the chat completions path, config.modelsPath the
// model list path.
class OpenAIChatClient final : public WireClient {
public:
  OpenAIChatClient(ClientConfig config, ChatQuirks quirks);
  Response request(const Json& anthropicBody, bool stream,
                   const OnEvent& onEvent = {},
                   std::stop_token stop = {},
                   const OnResponse& onResponse = {},
                   Timeout timeout = std::nullopt) override;
  Json listModels() override;
  std::string kind() override;
  const ChatQuirks& quirks() const { return quirks_; }
private:
  ChatQuirks quirks_;
  HttpSession session_;
};

} // namespace batbox::transport::openai_chat
