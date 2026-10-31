#pragma once
#include "app_state/State.hpp"

namespace batbox::app_state {
using SetAppState=std::function<void(const AppStateStore::Updater&)>;
struct PartnerViewServices {
  std::function<double()> nowMillis;
  std::function<std::optional<std::function<void()>>(const value::ValueRef&)>
    getAbortController;
};
bool isTerminalMissionStatus(const std::string& status);
bool isLocalBat(const value::ValueRef& mission);
value::ValueRef releaseLocalBat(const value::ValueRef& mission,
  const PartnerViewServices& services);
void enterPartnerView(const std::string& missionId,
  const SetAppState& setAppState,const PartnerViewServices& services);
void exitPartnerView(const SetAppState& setAppState,
  const PartnerViewServices& services);
void stopOrDismissBat(const std::string& missionId,
  const SetAppState& setAppState,const PartnerViewServices& services);
} // namespace batbox::app_state
