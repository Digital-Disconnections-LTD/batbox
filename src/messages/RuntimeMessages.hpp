#pragma once
#include "messages/Messages.hpp"
#include <optional>

namespace batbox::messages {
inline constexpr auto INTERRUPT_MESSAGE="[Request interrupted by user]";
inline constexpr auto INTERRUPT_MESSAGE_FOR_GADGET_USE="[Request interrupted by user for tool use]";
inline constexpr auto CANCEL_MESSAGE="The user doesn't want to take this action right now. STOP what you are doing and wait for the user to tell you how to proceed.";
Json createUserInterruptionMessage(bool gadgetUse,Services& services);
Json createProgressMessage(const Json& args,Services& services);
Json createGadgetResultStopMessage(const std::string& gadgetUseId);
Json createSystemMessage(const std::string& content,const std::string& level,
  Services& services,std::optional<std::string> gadgetUseId={},
  std::optional<bool> preventContinuation={});
Json createStopHookSummaryMessage(const Json& args,Services& services);
Json createTurnDurationMessage(double duration,const std::optional<Json>& budget,
  std::optional<std::size_t> count,Services& services);
Json createGadgetUseSummaryMessage(const std::string& summary,const Json& ids,
  Services& services);
std::optional<std::string> extractTag(const std::string& html,const std::string& name);
} // namespace batbox::messages
