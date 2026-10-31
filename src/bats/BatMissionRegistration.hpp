#pragma once
#include "bats/BatMissionNotification.hpp"

namespace batbox::bats {
struct BatRegistrationParams {
  std::string batId,description,poetry;
  Json selectedBat;
  std::optional<std::string> gadgetUseId;
  std::shared_ptr<std::stop_source> parentAbortOwner;
};
struct RegisteredLocalBat {
  Json missionState;
  std::shared_ptr<std::stop_source> abortOwner;
  std::shared_ptr<void> parentAbortLink;
  std::function<void()> unregisterCleanup;
};
struct BatRegistrationServices {
  BatMissionStateServices* state=nullptr;
  std::function<void(const std::string&,const RegisteredLocalBat&)> onRegistered;
  std::function<std::string(const std::string&)> getMissionOutputPath,
    getBatTranscriptPath;
  std::function<void(const std::string& missionId,
    const std::string& transcriptPath)> initMissionOutputAsSymlink;
  std::function<std::function<void()>(const std::function<void()>&)> registerCleanup;
  std::function<void(const Json&)> enqueueSdkEvent;
  std::function<void(bool child)> onCreateAbortOwner;
};
Json createMissionStateBase(const std::string& id,const std::string& type,
  const std::string& description,const std::optional<std::string>& gadgetUseId,
  BatRegistrationServices& services);
void registerMission(const Json& mission,BatRegistrationServices& services);
RegisteredLocalBat registerAsyncBat(const BatRegistrationParams& params,
  BatRegistrationServices& services);
} // namespace batbox::bats
