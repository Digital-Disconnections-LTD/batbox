#include "bats/ForkedBat.hpp"
#include <iostream>
#include <stdexcept>

using batbox::Json;
namespace ba=batbox::bats;

static void require(bool condition,const char* message){
  if(!condition)throw std::runtime_error(message);
}
int main(){
  auto appState=std::make_shared<Json>(Json{{"toolPermissionContext",{{"mode","default"}}}});
  auto full=std::make_shared<ba::BatContext>();
  full->query.data={{"marker","parent"},{"readFileState",{{"path","parent"}}}};
  full->query.contentReplacementState=batbox::gadget_results::createContentReplacementState();
  full->query.contentReplacementState->seenIds.add("parent-tool");
  full->query.contentReplacementState->replacements.set("parent-tool","shortened");
  full->getAppState=[appState]{return *appState;};
  full->setAppState=[appState](const std::function<Json(const Json&)>& update){
    *appState=update(*appState);
  };
  full->setAppStateForMissions=full->setAppState;
  batbox::execution::Context current=full->query;
  current.data["marker"]="iteration";
  const Json prefix=Json::array({Json{{"type","assistant"},{"uuid","a"}},
    Json{{"type","user"},{"uuid","u"}}});
  auto snapshot=ba::makeStashSafeSnapshot(full,Json::array({"system"}),
    Json{{"user","context"}},Json{{"system","context"}},current,prefix);
  require(snapshot->gadgetUseContext.query.data.at("marker")=="iteration",
    "iteration context was not captured");
  require(full->query.data.at("marker")=="parent","parent context was mutated");
  require(snapshot->forkContextMessages==prefix,"cache prefix was changed");
  require(snapshot->systemPoetry==Json::array({"system"}),"system prompt was changed");
  require(snapshot->gadgetUseContext.query.contentReplacementState==
    full->query.contentReplacementState,"cache snapshot lost replacement state identity");
  snapshot->gadgetUseContext.setAppState([](const Json& previous){
    Json next=previous;next["toolPermissionContext"]["mode"]="acceptEdits";return next;
  });
  require(full->getAppState().at("toolPermissionContext").at("mode")=="acceptEdits",
    "functional state callback lost its shared target");
  auto ptl=std::make_shared<ba::StashSafeParams>(*snapshot);
  ptl->forkContextMessages=Json::array({prefix.back()});
  require(snapshot->forkContextMessages==prefix,"PTL retry changed original snapshot");
  require(ptl->gadgetUseContext.getAppState().at("toolPermissionContext").at("mode")=="acceptEdits",
    "PTL retry lost native callback identity");
  ba::ContextServices services;
  services.cloneFileStateStash=[](const Json& value){return value;};
  services.createDenialTrackingState=[](){return Json::object();};
  services.createBatId=[](const auto&){return std::string("child");};
  services.randomUUID=[](){return std::string("chain");};
  ba::SubagentOverrides noOverride;
  auto child=ba::createSubagentContext(*full,noOverride,services);
  require(child.query.contentReplacementState &&
    child.query.contentReplacementState!=full->query.contentReplacementState,
    "subagent must clone parent replacement state");
  require(child.query.contentReplacementState->seenIds.contains("parent-tool"),
    "subagent clone lost previously seen tool IDs");
  child.query.contentReplacementState->seenIds.add("child-tool");
  require(!full->query.contentReplacementState->seenIds.contains("child-tool"),
    "child replacement mutation reached parent");
  auto resumed=batbox::gadget_results::createContentReplacementState();
  ba::SubagentOverrides override;
  override.indigenousContentReplacementState=resumed;
  auto resumedChild=ba::createSubagentContext(*full,override,services);
  require(resumedChild.query.contentReplacementState==resumed,
    "resume override replacement state must retain identity");
  std::cout<<"cache snapshot callback and PTL prefix isolation passed\n";
}
