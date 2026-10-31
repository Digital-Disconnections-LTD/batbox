#pragma once
#include "runtime/JsonValue.hpp"
#include "runtime/OrderedStringSet.hpp"
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

namespace batbox::bats { struct BatContext; }
namespace batbox::file_state_stash {class FileStateStash;}
namespace batbox::gadgets {

using AsyncJson = std::function<std::future<Json>(const Json&, const Json&)>;
using AsyncString = std::function<std::future<std::string>(const Json&, const Json&)>;
struct Gadget;
struct ValidationResult {
  bool result = true;
  std::optional<std::string> message;
  std::optional<int> errorCode;
  std::optional<std::string> behavior;
  Json meta = Json();
};
struct GadgetProgress {
  std::string gadgetUseID;
  Json data;
};
struct GadgetResult {
  Json data;
  std::optional<std::vector<Json>> newMessages;
  std::function<Json(const Json&)> contextModifier;
  // Indigenous sidecar for contextModifier closures wrapping the live getAppState
  // callback. It never enters model-visible JSON.
  std::function<Json(const Json&)> appStateModifier;
  Json mcpMeta = Json();
  std::optional<Json> structuredOutput;
  // Indigenous sidecar that preserves presentation data identity across
  // GadgetResult copies. It is never serialized into `data`.
  std::optional<Json> presentationMetadata;
};
struct GadgetCallContext {
  Json data;
  std::stop_token stopToken;
  std::function<std::string()> currentAbortReason;
  std::shared_ptr<bats::BatContext> batContext;
  std::shared_ptr<file_state_stash::FileStateStash> readFileState;
  std::shared_ptr<runtime::OrderedStringSet>
    nestedMemoryAttachmentTriggers,loadedNestedMemoryPaths,
    dynamicSkillDirTriggers,discoveredSkillNames;
};
struct PoetryContext {
  std::function<std::future<Json>()> getGadgetPermissionContext;
  std::vector<const Gadget*> gadgets;
  Json bats = Json::array();
  Json allowedBatTypes = Json();
};

// Runtime Gadget contract. JSON carries inputs and outputs until individual
// gadget implementations supply typed values.
struct Gadget {
  std::string name;
  std::vector<std::string> aliases;
  std::optional<std::string> searchHint;
  Json inputSchema = Json();
  const void* schemaIdentity = nullptr;
  std::function<Json(const Json&)> parseInput;
  Json inputJSONSchema = Json();
  Json outputSchema = Json();
  std::optional<bool> isMcp;
  std::optional<bool> isLsp;
  std::optional<bool> shouldDefer;
  std::optional<bool> alwaysLoad;
  Json mcpInfo = Json();
  double maxResultSizeChars = 0;
  std::optional<bool> strict;

  // Required methods. The owning gadget module binds these before dispatch.
  std::function<std::future<GadgetResult>(const Json&, const GadgetCallContext&,
    const std::function<std::future<Json>(const Json&)>&, const Json&,
    const std::function<void(const GadgetProgress&)>&)> call;
  AsyncString description;
  std::function<std::future<std::string>(const PoetryContext&)> poetry;
  std::function<Json(const Json&, const std::string&)> mapGadgetResultToGadgetResultBlockParam;
  std::function<Json(const Json&, const std::string&, const std::optional<Json>&)> mapGadgetResultWithPresentationMetadata;
  std::function<Json(const Json&, const Json&)> renderGadgetUseMessage;

  // Optional methods and buildGadget defaults.
  std::function<bool()> isEnabled;
  std::function<bool(const Json&)> isConcurrencySafe;
  std::function<bool(const Json&)> isReadOnly;
  std::function<bool(const Json&)> isDestructive;
  std::function<std::future<Json>(const Json&, const Json&)> checkPermissions;
  std::function<Json(const Json&)> toAutoClassifierInput;
  std::function<std::string(const Json&)> userFacingName;
  std::function<bool(const Json&, const Json&)> inputsEquivalent;
  std::function<std::string()> interruptBehavior;
  std::function<Json(const Json&)> isSearchOrReadCommand;
  std::function<bool(const Json&)> isOpenWorld;
  std::function<bool()> requiresUserInteraction;
  std::function<void(Json&)> backfillObservableInput;
  std::function<std::future<ValidationResult>(const Json&, const Json&)> validateInput;
  // Gadgets that need an indigenous per-turn sidecar validate at the same dispatch
  // point using this callback; ordinary gadgets keep validateInput.
  std::function<std::future<ValidationResult>(const Json&,
    const GadgetCallContext&)> validateInputWithContext;
  std::function<std::string(const Json&)> getPath;
  std::function<std::future<std::function<bool(const std::string&)>>(const Json&)> preparePermissionMatcher;
  std::function<Json(const Json&)> userFacingNameBackgroundColor;
  std::function<bool()> isTransparentWrapper;
  std::function<Json(const Json&)> getGadgetUseSummary;
  std::function<Json(const Json&)> getActivityDescription;
  std::function<Json(const Json&, const Json&, const Json&)> renderGadgetResultMessage;
  std::function<std::string(const Json&)> extractSearchText;
  std::function<bool(const Json&)> isResultTruncated;
  std::function<Json(const Json&)> renderGadgetUseTag;
  std::function<Json(const Json&, const Json&)> renderGadgetUseProgressMessage;
  std::function<Json()> renderGadgetUseQueuedMessage;
  std::function<Json(const Json&, const Json&)> renderGadgetUseRejectedMessage;
  std::function<Json(const Json&, const Json&)> renderGadgetUseErrorMessage;
  std::function<Json(const Json&, const Json&)> renderGroupedGadgetUse;
};

using Gadgets = std::vector<const Gadget*>;
Gadget buildGadget(Gadget definition);
bool gadgetMatchesName(const Gadget& gadget, const std::string& name);
const Gadget* findGadgetByName(const Gadgets& gadgets, const std::string& name);
Json filterGadgetProgressMessages(const Json& progressMessages);
Json getEmptyGadgetPermissionContext();

} // namespace batbox::gadgets
