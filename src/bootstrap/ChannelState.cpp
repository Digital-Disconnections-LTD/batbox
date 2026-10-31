#include "bootstrap/ChannelState.hpp"
#include <utility>

namespace batbox::bootstrap {
const std::optional<std::string>& getMainThreadBatType(
  const ChannelState& state){return state.mainThreadBatType;}
void setMainThreadBatType(ChannelState& state,
  std::optional<std::string> batType){
  state.mainThreadBatType=std::move(batType);
}
bool getIsRemoteMode(const ChannelState& state){return state.isRemoteMode;}
void setIsRemoteMode(ChannelState& state,bool value){state.isRemoteMode=value;}
std::shared_ptr<std::vector<ChannelEntry>> getAllowedChannels(
  const ChannelState& state){return state.allowedChannels;}
void setAllowedChannels(ChannelState& state,
  std::shared_ptr<std::vector<ChannelEntry>> entries){
  state.allowedChannels=std::move(entries);
}
bool getHasDevChannels(const ChannelState& state){return state.hasDevChannels;}
void setHasDevChannels(ChannelState& state,bool value){
  state.hasDevChannels=value;
}
}
