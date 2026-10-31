#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace batbox::bootstrap {
struct ChannelEntry {
  std::string kind,name;
  std::optional<std::string> marketplace;
  std::optional<bool> dev;
};
struct ChannelState {
  std::optional<std::string> mainThreadBatType;
  bool isRemoteMode=false;
  std::shared_ptr<std::vector<ChannelEntry>> allowedChannels=
    std::make_shared<std::vector<ChannelEntry>>();
  bool hasDevChannels=false;
};
const std::optional<std::string>& getMainThreadBatType(
  const ChannelState& state);
void setMainThreadBatType(ChannelState& state,
  std::optional<std::string> batType);
bool getIsRemoteMode(const ChannelState& state);
void setIsRemoteMode(ChannelState& state,bool value);
std::shared_ptr<std::vector<ChannelEntry>> getAllowedChannels(
  const ChannelState& state);
void setAllowedChannels(ChannelState& state,
  std::shared_ptr<std::vector<ChannelEntry>> entries);
bool getHasDevChannels(const ChannelState& state);
void setHasDevChannels(ChannelState& state,bool value);
}
