#pragma once
#include "app_state/State.hpp"
#include "runtime/OrderedStringSet.hpp"
#include "gadgets/Gadget.hpp"
#include "messages/Messages.hpp"
#include "gadget_results/State.hpp"
#include "file_state_stash/FileStateStash.hpp"
#include <functional>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

namespace batbox::execution {

struct Context {
  Json data = Json::object();
  gadgets::Gadgets gadgets;
  gadgets::Gadgets baseGadgets;
  // Indigenous descriptor lifetime follows copies of the query/bat context.
  std::shared_ptr<const void> gadgetSnapshotOwner;
  std::stop_token stop;
  std::string abortReason;
  std::function<Json(const Json&)> appStateModifier;
  std::function<app_state::AppStateRef(const app_state::AppStateRef&)>
    typedAppStateModifier;
  bool typedAppStateModifierComplete=true;
  std::shared_ptr<gadget_results::ContentReplacementState> contentReplacementState;
  // Shared indigenous stash for this execution context. It follows context
  // copies and is cloned when a subagent creates its own context.
  std::shared_ptr<file_state_stash::FileStateStash> readFileState;
  std::shared_ptr<runtime::OrderedStringSet>
    nestedMemoryAttachmentTriggers,loadedNestedMemoryPaths,
    dynamicSkillDirTriggers,discoveredSkillNames;
};

struct Update {
  std::optional<Json> message;
  std::optional<std::function<Json(const Json&)>> contextModifier;
  std::string gadgetUseID;
  std::optional<std::function<Json(const Json&)>> appStateModifier;
};

// Apply each Gadget's getAppState wrapper to a freshly read base state.
Json effectiveAppState(const Context& context,const Json& base);
void appendAppStateModifier(Context& context,
  const std::function<Json(const Json&)>& next);
void appendAppStateModifierPair(Context& context,
  const std::function<Json(const Json&)>& jsonNext,
  const std::function<app_state::AppStateRef(
    const app_state::AppStateRef&)>& typedNext);

struct Services {
  // Factories and imported hook/permission services. Production must bind
  // each callback required by the execution path it enables.
  std::function<Json(const Json&)> createUserMessage;
  std::function<Json(const Json&)> createAttachmentMessage;
  std::function<Json(const std::string& parentGadgetUseID,const Json& progress)> createProgressMessage;
  std::function<Json(const std::string& phase,const Json& infos,double durationMs)> createStopHookSummaryMessage;
  std::function<Json(const std::string&)> createGadgetResultStopMessage;
  std::function<std::string(const std::string&)> withMemoryCorrectionHint;
  std::string cancelMessage;
  std::function<std::string(const gadgets::Gadget&, const std::exception&)> formatValidationError;
  std::function<std::optional<std::string>(const gadgets::Gadget&, const Json&, const gadgets::Gadgets&)> schemaNotSentHint;
  std::function<void(const std::string&, const Json&, const Context&)> startSpeculativeClassifierCheck;
  std::function<Json(const Context&)> getAppState;
  // App binding retains the callback-bearing parent BatContext and refreshes
  // its query Context for this individual Gadget.call.
  std::function<std::shared_ptr<bats::BatContext>(const Context&,
    const Json& callData)> makeBatContext;
  std::function<void(const gadgets::Gadget&, const Json&, const Json&, const Context&,
                     const std::function<void(const Json&)>&)> preGadgetUseHooks;
  std::function<Json(const Json& hookPermissionResult, const gadgets::Gadget&, const Json&,
                     const Context&, const Json& assistantMessage, const std::string& gadgetUseID)> resolvePermissionDecision;
  std::function<void(const gadgets::Gadget&, const std::string&, const Json&, const Context&,
                     const std::function<void(const Json&)>&)> permissionDeniedHooks;
  std::function<void(const gadgets::Gadget&, const Json& input, const Json& output, const Context&,
                     const std::function<void(const Json&)>&)> postGadgetUseHooks;
  std::function<void(const gadgets::Gadget&, const Json& input, const std::string& error,bool isInterrupt,
                     const Context&, const std::function<void(const Json&)>&)> postGadgetUseFailureHooks;
  std::function<Json(const Json& block, const gadgets::Gadget&)> processPreMappedGadgetResultBlock;
  std::function<Json(const gadgets::Gadget&, const Json& output, const std::string& gadgetUseID)> processGadgetResultBlock;
  std::function<std::string(const std::exception&)> formatError;
  std::function<bool(const std::exception&)> isAbortError;
  std::function<std::optional<Json>(const std::exception&)> mcpMetaFromError;
  std::function<std::optional<std::string>(const std::exception&)> mcpAuthServerName;
  std::function<void(const std::function<Json(const Json&)>&)> updateAppState;
  std::function<std::future<Json>(const Json&)> canUseGadget;
  std::function<void(const std::string&)> markGadgetUseComplete;
  std::function<void(const std::string&)> markGadgetUseInProgress;
  std::function<void(bool)> setHasInterruptibleGadgetInProgress;
  std::function<std::string()> currentAbortReason;
  std::function<void(double)> addToGadgetDuration;
  std::function<std::int64_t()> nowMillis;
  std::string bashGadgetName = "Bash";
  bool transcriptClassifierFeature = false;
  bool internalUser = false;
};

// Dispatch a Gadget use through its selected execution path.
// Progress is emitted while Gadget.call is in flight; final results retain input order.
void runGadgetUse(const Json& gadgetUse, const Json& assistantMessage, Context& context,
                Services& services, const std::function<void(const Update&)>& yield);

} // namespace batbox::execution
