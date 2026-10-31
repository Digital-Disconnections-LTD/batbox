#pragma once
#include "application/Config.hpp"
#include "application_model/LocalModelService.hpp"
#include "execution/GadgetExecution.hpp"
#include "instructions/hooks/runtime/HookRuntimeBindings.hpp"
#include "instructions/hooks/runtime/EnvHooks.hpp"
#include "instructions/hooks/runtime/HookOutputParser.hpp"
#include "instructions/hooks/runtime/IndigenousCommandFull.hpp"
#include "instructions/hooks/runtime/StopPoetryWrappers.hpp"
#include "instructions/hooks/runtime/PoetryHookExecutor.hpp"
#include "instructions/hooks/runtime/BatHookExecutor.hpp"
#include "instructions/hooks/runtime/StructuredOutputEnforcement.hpp"
#include "instructions/hooks/runtime/FlightHookStore.hpp"
#include "query/StopHooks.hpp"
#include "query/AsyncEngine.hpp"
#include <future>
#include <map>
#include <memory>
#include <stop_token>

namespace batbox::application_permissions {class PermissionProvider;}
namespace batbox::bats {struct BatContext;}
namespace batbox::application_hooks {
namespace hooks=instructions::hooks::runtime;
struct IndigenousHookContext {
  gadgets::Gadgets gadgets;
  execution::Context query;
  std::shared_ptr<bats::BatContext> bat;
  std::shared_ptr<query::AsyncEngineServices> child;
};
struct Inputs {
  application::Config config;
  std::string flightId;
  std::function<std::string()> flightIdProvider;
  std::function<std::string(const std::string&)> transcriptPathForFlight;
  Json settings=Json::object();
  std::function<Json()> getAppState;
  std::shared_ptr<application_model::LocalModelService> model;
  std::shared_ptr<messages::Services> messages;
  std::function<gadgets::Gadgets()> currentGadgets;
  std::function<execution::Context()> currentContext;
  std::function<std::shared_ptr<query::AsyncEngineServices>(
    const bats::BatContext&,
    const std::function<std::future<Json>(const Json&)>&)> makeBatEngine;
  std::function<std::string(const std::string&)> batTranscriptPath;
  std::function<void(const std::function<Json(const Json&)>&)> setAppState;
  std::shared_ptr<application_permissions::PermissionProvider> permission;
  std::function<Json()> currentMessages;
  std::function<void(std::size_t)> addResponseLength;
  std::function<Json(const Json&)> createAttachmentMessage;
  std::function<void(const std::string&)> log;
  std::function<void(const Json&)> emitHookEvent,
    enqueuePendingNotification;
  std::function<std::string()> currentAbortReason;
  std::function<std::string()> randomUUID;
  double defaultTimeoutMs=600000;
};
// The owner provides both callbacks at the permission boundary. An absent
// rule result permits a hook allow result.
struct PermissionImports {
  std::function<std::future<Json>(const gadgets::Gadget&,const Json&,
    const execution::Context&,const Json&,const std::string&,
    const std::optional<Json>&)> canUseGadget;
  std::function<std::future<std::optional<Json>>(const gadgets::Gadget&,
    const Json&,const execution::Context&)> checkRuleBasedPermissions;
};
struct FlightStartOutput {
  Json messages=Json::array();
  std::optional<std::string> initialUserMessage;
  std::vector<std::string> watchPaths;
};
Json resolveHookPermissionDecision(const Json& hookPermissionResult,
  const gadgets::Gadget& gadget,const Json& input,const execution::Context& context,
  const Json& assistantMessage,const std::string& gadgetUseID,
  const PermissionImports& imports);

class Provider final:public std::enable_shared_from_this<Provider> {
public:
  Provider(event_loop::Loop& owner,Inputs inputs);
  void bindExecution(execution::Services& services,PermissionImports permission);
  void bindStop(query::StopHookServices& services);
  event_loop::Deferred<hooks::CompactHookOutput> preCompact(
    const std::string& trigger,const std::optional<std::string>& customInstructions,
    std::optional<std::stop_token> signal={});
  event_loop::Deferred<hooks::CompactHookOutput> postCompact(
    const std::string& trigger,const std::string& summary,
    std::optional<std::stop_token> signal={});
  // Null means no Elicitation hook decided the request. A blocking hook
  // returns {action:"decline"} to the request handler.
  event_loop::Deferred<Json> elicitationBefore(const std::string& server,
    const Json& params,std::stop_token signal={});
  event_loop::Deferred<Json> elicitationAfter(const std::string& server,
    const Json& params,const Json& response,std::stop_token signal={});
  void elicitationComplete(const std::string& server,
    const std::string& elicitationId);
  hooks::HookEventGenerator userPoetrySubmit(const std::string& poetry,
    const std::string& permissionMode,std::stop_token signal);
  event_loop::Deferred<std::vector<Json>> run(const std::string& event,
    Json detail,std::string matchQuery={},std::stop_token signal={},
    std::optional<execution::Context> queryContext={},
    std::optional<Json> hookMessages={});
  FlightStartOutput collectFlightStart(
    const std::vector<Json>& events)const;
  event_loop::Deferred<Json> asyncHookResponses();
  event_loop::Deferred<event_loop::Unit> finalizeAsyncHooks();
  event_loop::Deferred<hooks::EnvHookResult> fileChanged(
    const std::string& path,const std::string& event);
  event_loop::Deferred<hooks::EnvHookResult> cwdChanged(
    const std::string& oldCwd,const std::string& newCwd);
  event_loop::Deferred<event_loop::Unit> clearCwdEnvFiles();
  bool hasHooksFor(const std::string& event)const;
  bool hasActiveJobs()const;
  event_loop::Deferred<std::string> createWorktreeHook(
    const std::string& name);
  void registerFrontmatterHooks(const std::string& flightId,
    const Json& configured,const std::string& source,bool isBat,
    std::optional<std::string> skillRoot={});
  void clearFlightHooks(const std::string& flightId);
  std::string addFunctionHook(const std::string& flightId,
    const std::string& event,const std::string& matcher,
    std::function<hooks::FunctionHookReply(const Json&,std::stop_token)> callback,
    const std::string& errorMessage,std::optional<double> timeout={});
  void removeFunctionHook(const std::string& flightId,
    const std::string& event,const std::string& id);
private:
  hooks::HookYieldCursor makeSourceCursor(const std::string& event,
    Json detail,std::string matchQuery,std::stop_token signal,
    std::optional<execution::Context> queryContext,
    std::optional<Json> hookMessages);
  void streamSource(const std::string& event,Json detail,
    std::string matchQuery,std::stop_token signal,
    std::optional<execution::Context> queryContext,
    std::optional<Json> hookMessages,
    const std::function<bool(const Json&)>& sink);
  event_loop::Deferred<std::vector<Json>> runSource(const std::string& event,
    Json detail,std::string matchQuery,std::stop_token signal,
    std::optional<execution::Context> queryContext,
    std::optional<Json> hookMessages);
  Json baseInput(const std::optional<std::string>& mode={})const;
  std::vector<hooks::HookMatcher> settingsHooks(const std::string& event)const;
  std::vector<hooks::HookMatcher> flightHooks(const Json& state,
    const std::string& event)const;
  void configure();
  void bindCommandHooks();
  void bindHttpHooks();
  void bindPoetryHooks();
  void bindBatHooks();
  void bindFunctionHooks();
  void notifyElicitationResponse(const std::string& server,
    const std::string& action);
  event_loop::Loop& owner_;
  Inputs inputs_;
  hooks::HookConfigServices configServices_;
  hooks::HookMatchServices matcher_;
  hooks::ExecutorServices executor_;
  hooks::IndigenousCommandConfig command_;
  hooks::HookParserServices parser_;
  hooks::BaseHookInputServices baseServices_;
  std::shared_ptr<hooks::FlightEnvironment> flightEnvironment_;
  std::shared_ptr<hooks::AsyncHookRoster> asyncRoster_;
  std::shared_ptr<hooks::PoetryHookHost> poetryHost_;
  std::shared_ptr<hooks::BatHookHost> batHost_;
  hooks::FlightHooksState indigenousFlightHooks_;
  std::shared_ptr<hooks::FlightHookStore> indigenousFlightStore_;
  std::shared_ptr<hooks::FunctionHookHost> functionHost_;
  hooks::CompactHookServices compact_;
  std::map<std::string,std::map<std::string,std::vector<hooks::HookMatcher>>>
    registered_;
  void removeOneShot(const std::string& flightId,
    const std::string& event,const std::string& id);
};
} // namespace batbox::application_hooks
