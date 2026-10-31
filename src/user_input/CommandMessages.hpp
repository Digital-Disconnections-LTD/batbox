#pragma once
#include "messages/Messages.hpp"
#include <string>

namespace batbox::user_input {
std::string formatCommandInputTags(const std::string& name,const std::string& args);
Json createSyntheticUserCaveatMessage(messages::Services& services);
Json createUserInterruptionMessage(bool gadgetUse,messages::Services& services);
Json createCommandInputMessage(const std::string& content,messages::Services& services);
Json createSystemMessage(const std::string& content,const std::string& level,
  messages::Services& services,
  const std::optional<std::string>& gadgetUseID=std::nullopt,
  bool preventContinuation=false);
bool isSystemLocalCommandMessage(const Json& message);
} // namespace batbox::user_input
