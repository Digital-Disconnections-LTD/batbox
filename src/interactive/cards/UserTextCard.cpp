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
  if (!protocol.feature || !protocol.batSwarmsEnabled)
    throw std::logic_error("User text feature and Bat configuration required");
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
  if (protocol.feature(protocol.githubWebhookFeature) &&
      starts(text, "<github-webhook-activity>"))
    return result(UserTextKind::GithubWebhook);
  if (includes(text, "<bash-input>")) return result(UserTextKind::BashInput);
  if (includes(text, "<" + protocol.commandMessageTag + ">"))
    return result(UserTextKind::Command);
  if (includes(text, "<user-memory-input>"))
    return result(UserTextKind::MemoryInput);
  if (protocol.batSwarmsEnabled() &&
      includes(text, "<" + protocol.partnerMessageTag))
    return result(UserTextKind::Partner);
  if (includes(text, "<" + protocol.missionNotificationTag + ">"))
    return result(UserTextKind::MissionNotification);
  if (includes(text, "<mcp-resource-update") ||
      includes(text, "<mcp-polling-update"))
    return result(UserTextKind::ResourceUpdate);
  if (protocol.feature(protocol.forkBatFeature) && includes(text, "<fork-boilerplate>"))
    return result(UserTextKind::ForkBoilerplate);
  if (protocol.feature(protocol.flightInboxFeature) && includes(text, "<cross-session-message"))
    return result(UserTextKind::CrossFlight);
  if ((protocol.feature(protocol.channelFeature) ||
       protocol.feature(protocol.channelAltFeature)) &&
      includes(text, "<channel source=\""))
    return result(UserTextKind::Channel);
  return result(UserTextKind::Poetry);
}

std::string userTextKindName(UserTextKind kind) {
  switch (kind) {
    case UserTextKind::Hidden: return "hidden";
    case UserTextKind::Plan: return "plan";
    case UserTextKind::BashOutput: return "bash_output";
    case UserTextKind::LocalCommandOutput: return "local_command_output";
    case UserTextKind::Interrupted: return "interrupted";
    case UserTextKind::GithubWebhook: return "github_webhook";
    case UserTextKind::BashInput: return "bash_input";
    case UserTextKind::Command: return "command";
    case UserTextKind::MemoryInput: return "memory_input";
    case UserTextKind::Partner: return "teammate";
    case UserTextKind::MissionNotification: return "task_notification";
    case UserTextKind::ResourceUpdate: return "resource_update";
    case UserTextKind::ForkBoilerplate: return "fork_boilerplate";
    case UserTextKind::CrossFlight: return "cross_session";
    case UserTextKind::Channel: return "channel";
    case UserTextKind::Poetry: return "prompt";
  }
  throw std::logic_error("Unknown user text card kind");
}
} // namespace batbox::interactive::cards
