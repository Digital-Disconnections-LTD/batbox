#include "interactive/cards/UserTextCard.hpp"
#include "interactive/MessageTags.hpp"
#include <stdexcept>

namespace batbox::interactive::cards {
namespace {
bool starts(const std::string& text, const std::string& prefix) {
  return text.starts_with(prefix);
}
bool includes(const std::string& text, const std::string& part) {
  return text.find(part) != std::string::npos;
}
} // namespace

UserTextCard classifyUserText(const std::string& text,
                              const std::string& planContent,
                              const UserTextProtocol& protocol) {
  auto result = [&](UserTextKind kind) {
    return UserTextCard{kind, text, planContent};
  };
  if (value::trim(text) == protocol.noContentMessage) return result(UserTextKind::Hidden);
  if (!planContent.empty()) return result(UserTextKind::Plan);
  if (extractTag(text, protocol.tickTag)) return result(UserTextKind::Hidden);
  if (includes(text, "<" + protocol.localCommandCaveatTag + ">"))
    return result(UserTextKind::Hidden);
  if (starts(text, "<bash-stdout") || starts(text, "<bash-stderr"))
    return result(UserTextKind::BashOutput);
  if (starts(text, "<local-command-stdout") || starts(text, "<local-command-stderr"))
    return result(UserTextKind::LocalCommandOutput);
  if (text == protocol.interruptMessage || text == protocol.interruptGadgetMessage)
    return result(UserTextKind::Interrupted);
  if (includes(text, "<bash-input>")) return result(UserTextKind::BashInput);
  if (includes(text, "<" + protocol.commandMessageTag + ">"))
    return result(UserTextKind::Command);
  if (includes(text, "<user-memory-input>"))
    return result(UserTextKind::MemoryInput);
  if (includes(text, "<" + protocol.missionNotificationTag + ">"))
    return result(UserTextKind::MissionNotification);
  if (includes(text, "<mcp-resource-update") ||
      includes(text, "<mcp-polling-update"))
    return result(UserTextKind::ResourceUpdate);
  return result(UserTextKind::Poetry);
}

std::string userTextKindName(UserTextKind kind) {
  switch (kind) {
    case UserTextKind::Hidden: return "hidden";
    case UserTextKind::Plan: return "plan";
    case UserTextKind::BashOutput: return "bash_output";
    case UserTextKind::LocalCommandOutput: return "local_command_output";
    case UserTextKind::Interrupted: return "interrupted";
    case UserTextKind::BashInput: return "bash_input";
    case UserTextKind::Command: return "command";
    case UserTextKind::MemoryInput: return "memory_input";
    case UserTextKind::MissionNotification: return "task_notification";
    case UserTextKind::ResourceUpdate: return "resource_update";
    case UserTextKind::Poetry: return "prompt";
  }
  throw std::logic_error("Unknown user text card kind");
}
} // namespace batbox::interactive::cards
