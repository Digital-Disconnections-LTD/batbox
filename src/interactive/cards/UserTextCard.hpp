#pragma once

#include "runtime/JsonValue.hpp"
#include <functional>
#include <string>

namespace batbox::interactive::cards {

enum class UserTextKind {
  Hidden, Plan, BashOutput, LocalCommandOutput, Interrupted,
  GithubWebhook, BashInput, Command, MemoryInput, Partner,
  MissionNotification, ResourceUpdate, ForkBoilerplate,
  CrossFlight, Channel, Poetry
};

struct UserTextProtocol {
  std::string noContentMessage;
  std::string interruptMessage;
  std::string interruptGadgetMessage;
  std::string commandMessageTag;
  std::string localCommandCaveatTag;
  std::string tickTag;
  std::string partnerMessageTag;
  std::string missionNotificationTag;
  std::string githubWebhookFeature;
  std::string forkBatFeature;
  std::string flightInboxFeature;
  std::string channelFeature;
  std::string channelAltFeature;
  std::function<bool(const std::string&)> feature;
  std::function<bool()> batSwarmsEnabled;
};

struct UserTextCard {
  UserTextKind kind = UserTextKind::Hidden;
  std::string text;
  std::string planContent;
};

// Leaf card renderers consume the chosen route without reclassifying text.
UserTextCard classifyUserText(const std::string& text,
                              const std::string& planContent,
                              const UserTextProtocol& protocol);
std::string userTextKindName(UserTextKind kind);

} // namespace batbox::interactive::cards
