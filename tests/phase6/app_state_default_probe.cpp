#include "app_state/State.hpp"
#include <fstream>
#include <iostream>

using namespace batbox;
int main(int argc,char** argv){
  if(argc!=2)return 2;
  std::ifstream file(argv[1]);Json cases;file>>cases;
  Json rows=Json::array();
  for(const auto& item:cases){
    Json trace=Json::array();
    auto idle=std::make_shared<Json>(Json{{"status","idle"}});
    app_state::DefaultAppStateServices services;
    services.isPartner=[&]{trace.push_back("isTeammate");
      return item.at("teammate").get<bool>();};
    services.isPlanModeRequired=[&]{trace.push_back("isPlanModeRequired");
      return item.at("planRequired").get<bool>();};
    services.getInitialSettings=[&]{trace.push_back("getInitialSettings");
      return Json{{"model","local"}};};
    services.getEmptyGadgetPermissionContext=[&]{
      trace.push_back("getEmptyToolPermissionContext");
      return Json{{"mode","ask"},{"extra",true}};};
    services.createEmptyAttributionState=[&]{
      trace.push_back("createEmptyAttributionState");
      return Json{{"commits",Json::array()}};};
    services.shouldEnableThinkingByDefault=[&]{
      trace.push_back("shouldEnableThinkingByDefault");return Json(true);};
    services.shouldEnablePoetrySuggestion=[&]{
      trace.push_back("shouldEnablePromptSuggestion");return Json(false);};
    services.idleSpeculationState=idle;
    const auto state=app_state::getDefaultAppState(services);
    rows.push_back({{"name",item.at("name")},{"trace",trace},
      {"keys",state->ownKeys()},{"value",state->snapshot()},
      {"identities",{{"idle",state->fieldRef("speculation")==idle},
        {"agentMap",bool(state->batNameRoster)},
        {"hooksMap",bool(state->flightHooks)},
        {"trackedSet",bool(state->trackedFiles)},
        {"overlaysSet",bool(state->activeOverlays)}}}});
  }
  std::cout<<rows.dump()<<'\n';
}
