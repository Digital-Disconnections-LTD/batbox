#pragma once
#include "messages/Messages.hpp"
#include "gadgets/Gadget.hpp"
#include <functional>
#include <optional>
#include <stop_token>
#include <string>

namespace batbox::skills::remote {
struct DiscoveredSkill {std::string url;};
struct LoadedSkill {
  std::string skillPath,content;
  bool stashHit=false;
  double latencyMs=0;
};
struct Services {
  std::function<bool()> enabled;
  std::function<std::optional<std::string>(const std::string&)> stripCanonicalPrefix;
  std::function<std::optional<DiscoveredSkill>(const std::string&)>
    getDiscoveredRemoteSkill;
  std::function<LoadedSkill(const std::string&,const std::string&,std::stop_token)>
    loadRemoteSkill;
  std::function<void(const std::string&)> recordSkillUsage;
  std::function<void(const std::string&)> logDebug;
  std::function<std::string()> getFlightId;
  std::function<std::optional<std::string>()> currentBatId;
  std::function<void(const std::string&,const std::string&,
    const std::string&,const std::optional<std::string>&)> addInvokedSkill;
  messages::Services messageServices;
  std::string skillDirPlaceholder="${BATBOX_SKILL_DIR}";
  std::string flightIdPlaceholder="${BATBOX_SESSION_ID}";
  bool isWindows=false;
};
std::optional<std::string> remoteSlugForCommand(const std::string& commandName,
  Services& services);
std::optional<gadgets::ValidationResult> validateRemoteSkill(
  const std::string& commandName,Services& services);
gadgets::GadgetResult executeRemoteSkill(const std::string& slug,
  const std::string& commandName,const Json& parentMessage,
  Services& services,std::stop_token stopToken={});
}
