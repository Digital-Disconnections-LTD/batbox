#pragma once
#include "runtime/JsonValue.hpp"
#include <string>

namespace batbox::transport {
// Protocol names are configuration; product identity stays Batbox.
struct MessagesProtocol {
  std::string betaHeader = "anthropic-beta";
  std::string userProfileHeader = "batbox-user-profile-id";
  std::string roostHeader = "batbox-workspace-id";
};
struct MessageRequest { Json body; Json headers; };
Json transformOutputFormat(const Json& params);
void assertOneOutputFormat(const Json& params);
Json messageHeaders(const Json& params, const Json& overrides,
                    const MessagesProtocol& protocol = {});
MessageRequest prepareMessageRequest(const Json& params, const Json& overrides,
                                     const MessagesProtocol& protocol = {});
struct SerializedMessageRequest { std::string body; Json headers; };
// prepareMessageRequest with body["stream"]=stream, serialized; the common
// request without output_format is written straight from params.
SerializedMessageRequest serializeMessageRequest(const Json& params, bool stream,
  const Json& overrides, const MessagesProtocol& protocol = {});
} // namespace batbox::transport
