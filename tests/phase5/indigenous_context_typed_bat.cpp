#include "bats/ForkedBat.hpp"
#include "bats/TypedBatState.hpp"
#include "app_state/State.hpp"
#include <iostream>
#include <stdexcept>

using namespace batbox;
static void expect(bool condition,const char* name){
  if(!condition)throw std::runtime_error(name);
}
int main(){
  auto state=std::make_shared<app_state::AppState>();
  state->fields=std::make_shared<value::SharedRecord>();
  state->setPermissionContext(std::make_shared<value::SharedRecord>(
    value::SharedRecord::fromJson(Json{{"mode","default"}})));
  auto store=std::make_shared<app_state::AppStateStore>(state);
  int writes=0;
  bats::BatContext parent;
  parent.query.readFileState=file_state_stash::createFileStateStashWithSizeLimit(5);
  parent.query.nestedMemoryAttachmentTriggers=
    std::make_shared<runtime::OrderedStringSet>();
  parent.query.loadedNestedMemoryPaths=
    std::make_shared<runtime::OrderedStringSet>();
  parent.query.loadedNestedMemoryPaths->insert("parent-memory");
  parent.query.dynamicSkillDirTriggers=
    std::make_shared<runtime::OrderedStringSet>();
  parent.query.discoveredSkillNames=
    std::make_shared<runtime::OrderedStringSet>();
  parent.getAppState=[store]{return store->getState()->snapshot();};
  parent.getTypedAppState=[store]{return store->getState();};
  parent.setAppState=[](const auto&){};
  parent.setTypedAppState=[store,&writes](const app_state::AppStateUpdater& change){
    ++writes;store->setState(change);};
  parent.setTypedAppStateForMissions=parent.setTypedAppState;
  parent.updateTypedAttributionState=[](const auto&){};
  bats::ContextServices imports;
  imports.createDenialTrackingState=[]{return Json::object();};
  imports.createBatId=[](const auto&){return "child";};
  imports.randomUUID=[]{return "chain";};
  bats::SubagentOverrides overrides;
  auto child=bats::createSubagentContext(parent,overrides,imports);
  const auto childState=child.getTypedAppState();
  expect(childState!=store->getState(),"child state identity");
  expect(value::property(childState->snapshot()["toolPermissionContext"],
    "shouldAvoidPermissionPrompts")==true,"child permission override");
  expect(!store->getState()->snapshot()["toolPermissionContext"].contains(
    "shouldAvoidPermissionPrompts"),"parent permission isolation");
  expect(child.query.nestedMemoryAttachmentTriggers&&
    child.query.nestedMemoryAttachmentTriggers!=
      parent.query.nestedMemoryAttachmentTriggers&&
    child.query.loadedNestedMemoryPaths&&
    child.query.loadedNestedMemoryPaths->empty()&&
    child.query.dynamicSkillDirTriggers&&
    child.query.discoveredSkillNames&&
    child.query.discoveredSkillNames!=parent.query.discoveredSkillNames&&
    child.query.discoveredSkillNames->empty(),
    "child memory and skill Set isolation");
  child.setTypedAppState([](const auto& prior){return prior;});
  expect(writes==0,"child state setter disabled");
  child.setTypedAppStateForMissions([](const auto& prior){return prior;});
  expect(writes==1,"child task setter shared");
  auto stash=bats::makeStashSafeSnapshot(std::make_shared<bats::BatContext>(parent),
    Json::array(),Json::object(),Json::object(),parent.query,Json::array());
  expect(stash->gadgetUseContext.getTypedAppState()==state,
    "cache snapshot typed callback");
  overrides.getAppState=[]{return Json::object();};
  auto jsonOverride=bats::createSubagentContext(parent,overrides,imports);
  expect(!jsonOverride.getTypedAppState,
    "json-only override clears typed getter");
  bats::BatContext paired=parent;
  execution::appendAppStateModifierPair(paired.query,
    [](const Json& prior){
      auto next=prior;next["effortValue"]="high";return next;},
    [](const app_state::AppStateRef& prior){
      auto next=std::make_shared<app_state::AppState>(prior->shallowCopy());
      next->fields->set("effortValue","high");return next;});
  auto materialized=paired;
  bats::materializeAppStateModifier(materialized);
  expect(materialized.getTypedAppState()->snapshot().at("effortValue")=="high",
    "paired typed modifier applied");
  expect(!materialized.query.appStateModifier&&
    !materialized.query.typedAppStateModifier,
    "paired modifiers materialized");
  auto onlyJson=parent;
  execution::appendAppStateModifier(onlyJson.query,[](const Json& prior){
    return prior;});
  bats::materializeAppStateModifier(onlyJson);
  expect(!onlyJson.getTypedAppState,"unpaired JSON modifier clears typed getter");
  auto pairThenUnknown=paired;
  execution::appendAppStateModifier(pairThenUnknown.query,
    [](const Json& prior){return prior;});
  expect(!bats::effectiveTypedBatGetAppState(pairThenUnknown),
    "unknown modifier after pair invalidates typed getter");
  bats::materializeAppStateModifier(pairThenUnknown);
  expect(!pairThenUnknown.getTypedAppState,
    "unknown modifier after pair clears typed getter");
  auto unknownThenPair=parent;
  execution::appendAppStateModifier(unknownThenPair.query,
    [](const Json& prior){return prior;});
  execution::appendAppStateModifierPair(unknownThenPair.query,
    [](const Json& prior){return prior;},
    [](const app_state::AppStateRef& prior){return prior;});
  expect(!bats::effectiveTypedBatGetAppState(unknownThenPair),
    "later pair cannot repair unknown modifier");
  bats::materializeAppStateModifier(unknownThenPair);
  expect(!unknownThenPair.getTypedAppState,
    "unknown modifier before pair clears typed getter");
  auto directThenPair=parent;
  directThenPair.query.appStateModifier=[](const Json& prior){return prior;};
  execution::appendAppStateModifierPair(directThenPair.query,
    [](const Json& prior){return prior;},
    [](const app_state::AppStateRef& prior){return prior;});
  expect(!bats::effectiveTypedBatGetAppState(directThenPair),
    "direct legacy modifier before pair clears typed getter");
  std::cout<<"PASS typed agent state identity, task setter and snapshot\n";
}
