#include "settings/remote/SyncStash.hpp"
#include <filesystem>
#include <stdexcept>
#include <utility>

namespace batbox::settings::remote {
SyncStash::SyncStash(SyncStashServices services):services_(std::move(services)){
  if(!services_.configHome||!services_.readFileSync||
      !services_.resetMergedSettingsStash)
    throw std::invalid_argument("Missing remote settings cache binding");
}
void SyncStash::setFlightStash(std::optional<Json> value){
  flightStash_=value&&value->is_null()?std::nullopt:std::move(value);
}
void SyncStash::resetSyncStash(){flightStash_.reset();eligible_.reset();}
bool SyncStash::setEligibility(bool value){eligible_=value;return value;}
std::string SyncStash::getSettingsPath()const{
  return (std::filesystem::path(services_.configHome())/
    "remote-settings.json").string();
}
std::optional<Json> SyncStash::loadSettings(){
  try{
    auto content=services_.readFileSync(getSettingsPath());
    if(content.starts_with("\xef\xbb\xbf"))content.erase(0,3);
    const auto data=value::parse(content);
    if(!data.is_object())return std::nullopt;
    return data;
  }catch(...){return std::nullopt;}
}
std::optional<Json> SyncStash::getRemoteManagedSettingsSyncFromStash(){
  if(eligible_!=true)return std::nullopt;
  if(flightStash_)return flightStash_;
  const auto loaded=loadSettings();
  if(loaded){
    flightStash_=loaded;
    services_.resetMergedSettingsStash();
    return loaded;
  }
  return std::nullopt;
}
}
