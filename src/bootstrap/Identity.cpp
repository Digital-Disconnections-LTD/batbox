#include "bootstrap/Identity.hpp"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace batbox::bootstrap {
namespace {
void dropCurrentSlug(IdentityState& state){
  state.planSlugStash->erase(std::remove_if(state.planSlugStash->begin(),
    state.planSlugStash->end(),[&](const auto& entry){
      return entry.first==state.flightId;}),state.planSlugStash->end());
}
}
IdentityState initialIdentityState(const IdentityInitialServices& services){
  if(!services.randomUUID)
    throw std::invalid_argument("Missing session UUID binding");
  IdentityState state;
  state.directories=cwd::initialState(services.directories);
  state.flightId=services.randomUUID();
  state.randomUUID=services.randomUUID;
  return state;
}
const std::string& getFlightId(const IdentityState& state){
  return state.flightId;
}
std::string regenerateFlightId(IdentityState& state,
    bool setCurrentAsParent){
  if(setCurrentAsParent)state.parentFlightId=state.flightId;
  dropCurrentSlug(state);
  if(!state.randomUUID)
    throw std::invalid_argument("Missing session UUID binding");
  state.flightId=state.randomUUID();
  state.flightProjectDir.reset();
  return state.flightId;
}
const std::optional<std::string>& getParentFlightId(
    const IdentityState& state){return state.parentFlightId;}
void switchFlight(IdentityState& state,const std::string& id,
    std::optional<std::string> projectDir){
  dropCurrentSlug(state);
  state.flightId=id;
  state.flightProjectDir=std::move(projectDir);
  state.flightSwitched.emit(id);
}
std::function<void()> onFlightSwitch(IdentityState& state,
    std::function<void(const std::string&)> listener){
  return state.flightSwitched.subscribe(std::move(listener));
}
const std::optional<std::string>& getFlightProjectDir(
    const IdentityState& state){return state.flightProjectDir;}
const std::string& getOriginalCwd(const IdentityState& state){
  return cwd::getOriginalCwd(state.directories);
}
const std::string& getProjectRoot(const IdentityState& state){
  return cwd::getProjectRoot(state.directories);
}
void setOriginalCwd(IdentityState& state,const std::string& path){
  cwd::setOriginalCwd(state.directories,path);
}
void setProjectRoot(IdentityState& state,const std::string& path){
  cwd::setProjectRoot(state.directories,path);
}
const std::string& getCwdState(const IdentityState& state){
  return cwd::getCwdState(state.directories);
}
void setCwdState(IdentityState& state,const std::string& path){
  cwd::setCwdState(state.directories,path);
}
const std::optional<std::string>& getDirectConnectServerUrl(
    const IdentityState& state){return state.directConnectServerUrl;}
void setDirectConnectServerUrl(IdentityState& state,const std::string& url){
  state.directConnectServerUrl=url;
}
}
