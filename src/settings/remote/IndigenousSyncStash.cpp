#include "settings/remote/SyncStash.hpp"
#include "filesystem/FileOps.hpp"
#include <utility>

namespace batbox::settings::remote {
SyncStashServices indigenousSyncStashServices(
    std::function<std::string()> configHome,
    std::function<void()> resetMergedSettingsStash){
  return {std::move(configHome),[](const std::string& path){
    return filesystem::readFileSyncWithMetadata(path).content;
  },std::move(resetMergedSettingsStash)};
}
}
