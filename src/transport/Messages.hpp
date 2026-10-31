#pragma once
#include "runtime/JsonValue.hpp"
#include <string>

namespace batbox::transport {
// Protocol names are configuration; product identity stays Batbox.
struct MessagesProtocol {
  std::string betaHeader = "batbox-beta";
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
} // namespace batbox::transport
