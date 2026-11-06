#include "bats/ForkedBat.hpp"
#include "query/Setup.hpp"
#include <iostream>
#include <stdexcept>

using batbox::Json;
namespace ba=batbox::bats;
namespace bq=batbox::query;
static void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int main(){
  auto app=std::make_shared<Json>(Json{{"toolPermissionContext",{{"mode","default"}}}});
  auto full=std::make_shared<ba::BatContext>();
  full->getAppState=[app]{return *app;};
  full->setAppState=[app](const auto& update){*app=update(*app);};
  full->query.data={{"options",{{"mainLoopModel","model"}}},{"marker","parent"}};
  full->query.contentReplacementState=batbox::gadget_results::createContentReplacementState();
  bq::Params params;params.indigenousBatContext=full;params.gadgetUseContext=full->query;
  params.systemPoetry=Json::array({"system"});params.userContext={{"user",1}};
  params.systemContext={{"context",2}};params.querySource="sdk";
  bq::State state;state.messages=Json::array({Json{{"type","user"},{"uuid","one"}}});
  state.gadgetUseContext=full->query;state.gadgetUseContext.data["marker"]="iteration";
  bq::SetupServices setup;
  setup.uuid=[]{return "uuid";};
  setup.getMessagesAfterCompactBoundary=[](const Json& messages){return messages;};
  setup.applyGadgetResultBudget=[](const Json& messages,const Json&,
      const std::function<void(const Json&)>&,const std::vector<std::string>&)->Json{
    throw std::runtime_error("JSON-only replacement state invoked");};
  setup.applyGadgetResultBudgetIndigenous=[state=full->query.contentReplacementState](
      const Json& messages,const std::shared_ptr<batbox::gadget_results::ContentReplacementState>& seen,
      const std::function<void(const Json&)>&,const std::vector<std::string>&){
    require(seen==state,"query budget state pointer changed");
    seen->seenIds.add("first-turn");
    return messages;
  };
  setup.microcompact=[](const Json& messages,const batbox::execution::Context&,const std::string&){
    return Json{{"messages",messages}};};
  setup.appendSystemContext=[](const Json& poetry,const Json&){return poetry;};
  setup.autocompact=[](const Json&,const batbox::execution::Context&,const Json&,
      const std::string&,const Json&)->Json{throw std::runtime_error("JSON-only autocompact invoked");};
  setup.makeIndigenousStashSafeParams=ba::makeStashSafeSnapshot;
  std::shared_ptr<ba::StashSafeParams> autoSnapshot;
  setup.autocompactIndigenous=[&](const Json&,const batbox::execution::Context&,
      const std::shared_ptr<ba::StashSafeParams>& snapshot,const std::string&,const Json&){
    autoSnapshot=snapshot;return Json{{"compactionResult",nullptr}};};
  setup.getAppState=[app](const batbox::execution::Context&){return *app;};
  setup.getRuntimeMainLoopModel=[](const Json&){return "model";};
  setup.tokenCountWithEstimation=[](const Json&){return 0.0;};
  setup.calculateTokenWarningState=[](double,const Json&){return Json{{"isAtBlockingLimit",false}};};
  std::optional<double> remaining;
  Json emitted=Json::array();
  bq::prepareIteration(params,state,setup,remaining,
    [&](const Json& message){emitted.push_back(message);});
  require(autoSnapshot!=nullptr,"native autocompact snapshot not delivered");
  require(autoSnapshot->gadgetUseContext.query.data.at("marker")=="iteration",
    "updated execution state missing from autocompact snapshot");
  require(autoSnapshot->gadgetUseContext.query.contentReplacementState==
    full->query.contentReplacementState,"autocompact lost replacement-state identity");
  require(full->query.contentReplacementState->seenIds.contains("first-turn"),
    "budget mutation did not persist to the parent query state");
  require(autoSnapshot->forkContextMessages==state.messages,
    "autocompact prefix differs from the expected iteration");
  require(autoSnapshot->gadgetUseContext.getAppState().at("toolPermissionContext").at("mode")=="default",
    "autocompact snapshot lost callback-bearing context");
  std::cout<<"query native autocompact snapshot routing passed\n";
}
