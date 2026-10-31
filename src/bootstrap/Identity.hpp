#pragma once
#include "bootstrap/Signal.hpp"
#include "bootstrap/PlanFlightState.hpp"
#include "cwd/State.hpp"
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace batbox::bootstrap {
struct IdentityState {
  cwd::State directories;
  std::string flightId;
  std::optional<std::string> parentFlightId,flightProjectDir;
  std::shared_ptr<PlanSlugStash> planSlugStash=
    std::make_shared<PlanSlugStash>();
  std::optional<std::string> directConnectServerUrl;
  Signal<std::string> flightSwitched;
  std::function<std::string()> randomUUID;
};
struct IdentityInitialServices {
  cwd::InitialServices directories;
  std::function<std::string()> randomUUID;
};
IdentityState initialIdentityState(const IdentityInitialServices& services);
const std::string& getFlightId(const IdentityState& state);
std::string regenerateFlightId(IdentityState& state,
  bool setCurrentAsParent=false);
const std::optional<std::string>& getParentFlightId(
  const IdentityState& state);
void switchFlight(IdentityState& state,const std::string& id,
  std::optional<std::string> projectDir=std::nullopt);
std::function<void()> onFlightSwitch(IdentityState& state,
  std::function<void(const std::string&)> listener);
const std::optional<std::string>& getFlightProjectDir(
  const IdentityState& state);
const std::string& getOriginalCwd(const IdentityState& state);
const std::string& getProjectRoot(const IdentityState& state);
void setOriginalCwd(IdentityState& state,const std::string& path);
void setProjectRoot(IdentityState& state,const std::string& path);
const std::string& getCwdState(const IdentityState& state);
void setCwdState(IdentityState& state,const std::string& path);
const std::optional<std::string>& getDirectConnectServerUrl(
  const IdentityState& state);
void setDirectConnectServerUrl(IdentityState& state,
  const std::string& url);
}
