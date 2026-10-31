#pragma once
#include "messages/Messages.hpp"

namespace batbox::application_cli {
Json toSdkCompactMetadata(const Json& metadata);
std::string localOutputText(const std::string& raw);
Json localCommandOutputToSdkAssistantMessage(const std::string& raw,
  const Json& uuid,const std::string& flightId,messages::Services& messages);
}
