#pragma once
#include "application/Config.hpp"
#include "application_model/LocalModelService.hpp"
#include "bats/ForkedBat.hpp"
#include "messages/Context.hpp"
#include "query/AsyncCompletion.hpp"
#include "query/AsyncGadgetTurn.hpp"
#include "query/StopHooks.hpp"
#include "gadget_results/Budget.hpp"
#include <set>

namespace batbox::application_engine {
// The executable binds the imported application features here. The query,
// model, gadget scheduling and recovery state machines retain their own owners.
struct Inputs {
  event_loop::Loop& owner;
  application::Config config;
  std::string flightId;
  std::function<std::string()> flightIdProvider;
  std::shared_ptr<application_model::LocalModelService> model;
  std::shared_ptr<messages::Services> messages;
  std::shared_ptr<execution::Services> execution;
  gadget_results::BudgetImports budget;
  std::set<std::string> features;
  std::function<Json(const execution::Context&)> getAppState;
  std::function<bool()> autoCompactEnabled;
  std::function<Json()> compactionSettings;
  // Rebind the hook permission resolver with each child's live app-state and
  // permission callback. The main resolver captures the main query context.
  std::function<void(execution::Services&,
    const std::function<Json(const execution::Context&)>&,
    const std::function<std::future<Json>(const Json&)>&)> bindBatPermission;
  std::function<Json(const Json&,const gadgets::Gadgets&)> normalize;
  std::function<Json(const Json&,const Json&)> appendSystemContext,
    prependUserContext;
  std::function<event_loop::Deferred<Json>(const Json&,
    const execution::Context&,const std::string&)> microcompact;
  std::function<event_loop::Deferred<Json>(const Json&,
    const execution::Context&,const Json&,const std::string&,
    const Json&,double)> autocompact;
  std::function<event_loop::Deferred<Json>(const Json&,
    const execution::Context&,const std::shared_ptr<bats::StashSafeParams>&,
    const std::string&,const Json&,double)> autocompactIndigenous;
  std::function<void(const Json&,const Json&)> recordContentReplacement;
  std::function<double(const Json&)> tokenCount,finalContextTokens;
  std::function<void(const std::string&)> checkpoint;
  std::function<void(const std::exception&)> logError;
  std::function<void(const Json&)> addNotification;
  query::StopHookServices stopHooks;
  query::AsyncCompletionServices completion;
  query::AsyncGadgetTurnServices gadgetTurn;
  std::function<query::MemoryPrefetch(const Json&,const execution::Context&)>
    startMemoryPrefetch;
  std::function<void(const Json&,const query::Params&,
    const execution::Context&)> postSamplingHooks;
  std::function<event_loop::Deferred<Json>(const Json&,const execution::Context&)>
    refreshUserContextAfterCompact;
  explicit Inputs(event_loop::Loop& loop):owner(loop){}
};
class Engine final:public std::enable_shared_from_this<Engine> {
public:
  static std::shared_ptr<Engine> create(Inputs inputs);
  std::shared_ptr<query::AsyncEngineServices> services()const{
    return {shared_from_this(),services_.get()};
  }
  std::shared_ptr<Engine> forBat(const bats::BatContext& context,
    std::function<std::future<Json>(const Json&)> canUseGadget)const;
private:
  explicit Engine(Inputs inputs):inputs_(std::move(inputs)){}
  void initialize();
  void bindSetup();
  void bindModel();
  void bindCompletion();
  void bindGadgets();
  Inputs inputs_;
  std::shared_ptr<query::AsyncEngineServices> services_;
};
query::AsyncStopCursor makeStopCursor(event_loop::Loop& owner,
  const Json& messages,const Json& assistants,const query::Params& params,
  const execution::Context& context,std::optional<bool> active,
  query::StopHookServices services);
} // namespace batbox::application_engine
