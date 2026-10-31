#include "model/Preflight.hpp"
#include "model/ModelWrappers.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::model {
namespace {
const Json& field(const Json& value, const char* key) { return value::property(value, key); }
std::string stringField(const Json& value, const char* key) {
  const auto& part = field(value, key);
  return part.is_string() ? part.get<std::string>() : std::string{};
}
bool booleanField(const Json& value, const char* key) { return value::truthy(field(value, key)); }
bool contains(const Json& array, const Json& value) {
  return std::find(array.begin(), array.end(), value) != array.end();
}
void require(bool ready, const char* name) {
  if (!ready) throw std::invalid_argument(std::string("Missing query preflight dependency: ") + name);
}
} // namespace

namespace {
PreflightState prepareQueryModelImpl(const PreflightInputs& input,
    const PreflightServices& services,std::optional<PreflightState> seeded,
    bool stopBeforeGadgetSearch=false,
    std::optional<bool> gadgetSearchDecision=std::nullopt,
    bool stopBeforeStashedImports=false,
    bool stopBeforeSchemas=false,
    std::optional<Json> schemaOverride=std::nullopt) {
  const std::string model = stringField(input.options, "model");
  const std::string querySource = stringField(input.options, "querySource");
  if(!seeded){
    require(static_cast<bool>(services.isSubscriber), "isSubscriber");
    require(static_cast<bool>(services.isNonCustomPrimaryModel), "isNonCustomPrimaryModel");
    require(static_cast<bool>(services.offSwitchActivated), "offSwitchActivated");
  }
  require(static_cast<bool>(services.getAPIProvider), "getAPIProvider");
  require(static_cast<bool>(services.getMergedBetas), "getMergedBetas");
  require(static_cast<bool>(services.isAdvisorEnabled), "isAdvisorEnabled");
  if(!stopBeforeGadgetSearch){
    if(!gadgetSearchDecision&&!(seeded&&seeded->gadgetSearchPrepared))
      require(static_cast<bool>(services.isGadgetSearchEnabled), "isToolSearchEnabled");
    require(static_cast<bool>(services.isDeferredGadget), "isDeferredTool");
    require(static_cast<bool>(services.gadgetMatchesName), "toolMatchesName");
    require(static_cast<bool>(services.stashedMicrocompactFeature), "cachedMicrocompactFeature");
  }
  if(!stopBeforeGadgetSearch&&!stopBeforeStashedImports&&!stopBeforeSchemas){
    if(!schemaOverride)require(static_cast<bool>(services.gadgetToAPISchema), "toolToAPISchema");
    require(static_cast<bool>(services.prepareMessagesForRequest), "prepareMessagesForRequest");
    require(static_cast<bool>(services.buildSystemPoetryBlocks), "buildSystemPromptBlocks");
    require(static_cast<bool>(services.getPoetryStashingEnabled), "getPromptCachingEnabled");
    require(static_cast<bool>(services.resolveAppliedEffort), "resolveAppliedEffort");
    require(static_cast<bool>(services.getCLISyspromptPrefix), "getCLISyspromptPrefix");
    require(static_cast<bool>(services.getHeaderLatch), "getHeaderLatch");
    require(static_cast<bool>(services.transcriptClassifierFeature), "transcriptClassifierFeature");
    require(static_cast<bool>(services.isFastModeEnabled), "isFastModeEnabled");
    require(static_cast<bool>(services.isFastModeAvailable), "isFastModeAvailable");
    require(static_cast<bool>(services.isFastModeCooldown), "isFastModeCooldown");
    require(static_cast<bool>(services.isFastModeSupportedByModel), "isFastModeSupportedByModel");
  }
  if(!stopBeforeGadgetSearch&&!stopBeforeStashedImports)
    require(static_cast<bool>(services.shouldUseGlobalStashScope), "shouldUseGlobalCacheScope");
  PreflightState state=seeded?std::move(*seeded):PreflightState{};

  if(!seeded){
    // Resolve the locally configured off-switch decision.
    if (!services.isSubscriber() && services.isNonCustomPrimaryModel(model) &&
        services.offSwitchActivated()) {
      state.offSwitch = true;
      require(static_cast<bool>(services.offSwitchMessage), "offSwitchMessage");
      state.offSwitchMessage = services.offSwitchMessage(model);
      return state;
    }

    state.previousRequestId = getPreviousRequestIdFromMessages(input.messages);
    state.providerAtStart = services.getAPIProvider();
    if (state.providerAtStart == "bedrock" && model.find("application-inference-profile") != std::string::npos) {
      require(static_cast<bool>(services.getInferenceProfileBackingModel), "getInferenceProfileBackingModel");
      state.resolvedModel = services.getInferenceProfileBackingModel(model).value_or(model);
    } else state.resolvedModel = model;
  }
  if(state.offSwitch)return state;
  if(!state.preGadgetSearchPrepared){
  if(services.queryCheckpoint)services.queryCheckpoint("query_tool_schema_build_start");
  state.isAgenticQuery = querySource.starts_with("repl_main_thread") ||
    querySource.starts_with("agent:") || querySource == "sdk" ||
    querySource == "hook_agent" || querySource == "verification_agent";
  state.betas = services.getMergedBetas(model, state.isAgenticQuery);
  if (services.isAdvisorEnabled && services.isAdvisorEnabled() &&
      !services.advisorBetaHeader.empty()) state.betas.push_back(services.advisorBetaHeader);
  if (state.isAgenticQuery && services.isAdvisorEnabled()) {
    std::string advisorOption = stringField(input.options, "advisorModel");
    require(static_cast<bool>(services.getExperimentAdvisorModels), "getExperimentAdvisorModels");
    {
      const auto experiment = services.getExperimentAdvisorModels();
      if (experiment) require(static_cast<bool>(services.normalizeModelStringForAPI), "normalizeModelStringForAPI");
      if (experiment &&
          services.normalizeModelStringForAPI(stringField(*experiment, "baseModel")) ==
          services.normalizeModelStringForAPI(model))
        advisorOption = stringField(*experiment, "advisorModel");
    }
    if (!advisorOption.empty()) {
      require(static_cast<bool>(services.parseUserSpecifiedModel), "parseUserSpecifiedModel");
      require(static_cast<bool>(services.normalizeModelStringForAPI), "normalizeModelStringForAPI");
      require(static_cast<bool>(services.modelSupportsAdvisor), "modelSupportsAdvisor");
      require(static_cast<bool>(services.isValidAdvisorModel), "isValidAdvisorModel");
      const auto normalized = services.normalizeModelStringForAPI(services.parseUserSpecifiedModel(advisorOption));
      if (services.modelSupportsAdvisor(model) && services.isValidAdvisorModel(normalized))
        state.advisorModel = normalized;
    }
  }
  state.preGadgetSearchPrepared=true;
  }
  if(stopBeforeGadgetSearch)return state;

  if(!state.gadgetSearchPrepared){
  state.useGadgetSearch = gadgetSearchDecision.value_or(false);
  if(!gadgetSearchDecision)
    state.useGadgetSearch=services.isGadgetSearchEnabled(model,input.gadgets,input.options);
  if (state.useGadgetSearch) {
    for (const Json& gadget : input.gadgets)
      if (services.isDeferredGadget(gadget)) state.deferredGadgetNames.insert(stringField(gadget, "name"));
  }
  if (state.useGadgetSearch && state.deferredGadgetNames.empty() &&
      !booleanField(input.options, "hasPendingMcpServers")) state.useGadgetSearch = false;

  std::set<std::string> discovered;
  if (state.useGadgetSearch) {
    require(static_cast<bool>(services.extractDiscoveredGadgetNames), "extractDiscoveredToolNames");
    require(static_cast<bool>(services.shouldDeferLspGadget), "shouldDeferLspTool");
    discovered = services.extractDiscoveredGadgetNames(input.messages);
  }
  for (const Json& gadget : input.gadgets) {
    if (state.useGadgetSearch) {
      if (!state.deferredGadgetNames.contains(stringField(gadget, "name")) ||
          services.gadgetMatchesName(gadget, services.gadgetSearchGadgetName) ||
          discovered.contains(stringField(gadget, "name"))) state.filteredGadgets.push_back(gadget);
    } else if (!services.gadgetMatchesName(gadget, services.gadgetSearchGadgetName)) state.filteredGadgets.push_back(gadget);
  }
  if (state.useGadgetSearch) {
    require(static_cast<bool>(services.getGadgetSearchBetaHeader), "getToolSearchBetaHeader");
    state.gadgetSearchHeader = services.getGadgetSearchBetaHeader();
  }
  if (state.gadgetSearchHeader && services.getAPIProvider() != "bedrock" && !contains(state.betas, *state.gadgetSearchHeader))
    state.betas.push_back(*state.gadgetSearchHeader);
  state.gadgetSearchPrepared=true;
  }

  if(!state.stashedFeatureEvaluated){
    state.stashedFeature=services.stashedMicrocompactFeature();
    state.stashedFeatureEvaluated=true;
  }
  if(stopBeforeStashedImports)return state;
  if(!state.beforeSchemasPrepared){
  if (state.stashedFeature) {
    require(static_cast<bool>(services.isStashedMicrocompactEnabled), "isCachedMicrocompactEnabled");
    require(static_cast<bool>(services.isModelSupportedForStashEditing), "isModelSupportedForCacheEditing");
    require(static_cast<bool>(services.stashEditingBetaHeader), "cacheEditingBetaHeader");
    state.stashedMCEnabled = services.isStashedMicrocompactEnabled() &&
      services.isModelSupportedForStashEditing(model);
    state.stashEditingHeader = services.stashEditingBetaHeader();
  }

  state.useGlobalStashFeature = services.shouldUseGlobalStashScope();
  if (state.useGlobalStashFeature) {
    for (const Json& gadget : state.filteredGadgets) {
      if (field(gadget, "isMcp") == true &&
          !shouldDeferGadgetForPreflight(state,gadget,services)) {
        state.needsGadgetBasedStashMarker = true;
        break;
      }
    }
    if (!services.poetryStashingScopeBetaHeader.empty() &&
        !contains(state.betas, services.poetryStashingScopeBetaHeader))
      state.betas.push_back(services.poetryStashingScopeBetaHeader);
  }
  state.beforeSchemasPrepared=true;
  }
  if(stopBeforeSchemas)return state;
  if(schemaOverride)state.gadgetSchemas=std::move(*schemaOverride);
  else for (const Json& gadget : state.filteredGadgets)
    state.gadgetSchemas.push_back(services.gadgetToAPISchema(gadget,
      shouldDeferGadgetForPreflight(state,gadget,services),input.gadgets,input.options));

  if(services.queryCheckpoint)services.queryCheckpoint("query_tool_schema_build_end");
  if(services.queryCheckpoint)services.queryCheckpoint("query_message_normalization_start");
  state.messagesForAPI = services.prepareMessagesForRequest(input.messages, state.filteredGadgets,
    state.useGadgetSearch, contains(state.betas, services.advisorBetaHeader));
  if(services.queryCheckpoint)services.queryCheckpoint("query_message_normalization_end");
  // Prepend deferred Gadget metadata after normalization, before system blocks.
  if (state.useGadgetSearch) require(static_cast<bool>(services.isDeferredGadgetsDeltaEnabled), "isDeferredToolsDeltaEnabled");
  if (state.useGadgetSearch && !services.isDeferredGadgetsDeltaEnabled()) {
    require(static_cast<bool>(services.formatDeferredGadgetLine), "formatDeferredToolLine");
    require(static_cast<bool>(services.createMetaUserMessage), "createMetaUserMessage");
    std::vector<std::string> deferredLines;
    for (const Json& gadget : input.gadgets)
      if (state.deferredGadgetNames.contains(stringField(gadget, "name")))
        deferredLines.push_back(services.formatDeferredGadgetLine(gadget));
    std::sort(deferredLines.begin(), deferredLines.end());
    std::string joined;
    for (const auto& line : deferredLines) {
      if (!joined.empty()) joined += '\n';
      joined += line;
    }
    if (!joined.empty()) {
      Json prepended = Json::array();
      prepended.push_back(services.createMetaUserMessage(
        "<available-deferred-tools>\n" + joined + "\n</available-deferred-tools>"));
      for (const auto& message : state.messagesForAPI) prepended.push_back(message);
      state.messagesForAPI = std::move(prepended);
    }
  }

  bool hasChromeGadgets = false;
  if (!state.filteredGadgets.empty()) require(static_cast<bool>(services.isChromeGadget), "isChromeTool");
  for (const Json& gadget : state.filteredGadgets)
    if (services.isChromeGadget(stringField(gadget, "name"))) { hasChromeGadgets = true; break; }
  if (state.useGadgetSearch && hasChromeGadgets)
    require(static_cast<bool>(services.isMcpInstructionsDeltaEnabled), "isMcpInstructionsDeltaEnabled");
  const bool injectChromeHere = state.useGadgetSearch && hasChromeGadgets &&
    !services.isMcpInstructionsDeltaEnabled();
  state.systemPoetry = Json::array();
  const std::string prefix = services.getCLISyspromptPrefix(input.options);
  if (!prefix.empty()) state.systemPoetry.push_back(prefix);
  for (const auto& segment : input.systemPoetry) if (value::truthy(segment)) state.systemPoetry.push_back(segment);
  if (state.advisorModel && !services.advisorInstructions.empty())
    state.systemPoetry.push_back(services.advisorInstructions);
  if (injectChromeHere && !services.chromeInstructions.empty())
    state.systemPoetry.push_back(services.chromeInstructions);
  const bool poetryStashing = input.options.contains("enablePromptCaching") &&
    !input.options["enablePromptCaching"].is_null()
      ? input.options["enablePromptCaching"].get<bool>() : services.getPoetryStashingEnabled(model);
  state.system = services.buildSystemPoetryBlocks(state.systemPoetry, poetryStashing,
    state.needsGadgetBasedStashMarker, querySource);

  Json extraSchemas = field(input.options, "extraToolSchemas");
  if (!extraSchemas.is_array()) extraSchemas = Json::array();
  if (state.advisorModel) extraSchemas.push_back(
    Json{{"type", "advisor_20260301"}, {"name", "advisor"}, {"model", *state.advisorModel}});
  state.allGadgets = state.gadgetSchemas;
  for (const auto& schema : extraSchemas) state.allGadgets.push_back(schema);

  state.isFastMode = services.isFastModeEnabled() &&
    services.isFastModeAvailable() &&
    !services.isFastModeCooldown() &&
    services.isFastModeSupportedByModel(model) &&
    booleanField(input.options, "fastMode");
  const auto latched = [&](const char* name) {
    return services.getHeaderLatch(name);
  };
  bool afkLatched = latched("afk");
  const bool transcriptFeatureForLatch = services.transcriptClassifierFeature();
  if (transcriptFeatureForLatch &&
      !afkLatched && state.isAgenticQuery) {
    require(static_cast<bool>(services.shouldIncludeFirstPartyOnlyBetas), "shouldIncludeFirstPartyOnlyBetas");
    require(static_cast<bool>(services.isAutoModeActive), "isAutoModeActive");
  }
  if (transcriptFeatureForLatch &&
      !afkLatched && state.isAgenticQuery &&
      services.shouldIncludeFirstPartyOnlyBetas() && services.isAutoModeActive()) {
    afkLatched = true;
    require(static_cast<bool>(services.setHeaderLatch), "setHeaderLatch");
    services.setHeaderLatch("afk", true);
  }
  bool fastLatched = latched("fastMode");
  if (!fastLatched && state.isFastMode) {
    fastLatched = true;
    require(static_cast<bool>(services.setHeaderLatch), "setHeaderLatch");
    services.setHeaderLatch("fastMode", true);
  }
  bool editingLatched = latched("cacheEditing");
  if (state.stashedFeature && !editingLatched && state.stashedMCEnabled &&
      services.getAPIProvider() == "firstParty" && querySource == "repl_main_thread") {
    editingLatched = true;
    require(static_cast<bool>(services.setHeaderLatch), "setHeaderLatch");
    services.setHeaderLatch("cacheEditing", true);
  }
  bool thinkingLatched = latched("thinkingClear");
  if (!thinkingLatched && state.isAgenticQuery) {
    require(static_cast<bool>(services.getLastApiCompletionTimestamp), "getLastApiCompletionTimestamp");
    auto last = services.getLastApiCompletionTimestamp();
    if (last) require(static_cast<bool>(services.nowMillis), "nowMillis");
    if (last && services.nowMillis() - *last > services.stashTtlOneHourMs) {
      thinkingLatched = true;
      require(static_cast<bool>(services.setHeaderLatch), "setHeaderLatch");
      services.setHeaderLatch("thinkingClear", true);
    }
  }
  Json effort = services.resolveAppliedEffort(model, field(input.options, "effortValue"));

  if(services.poetryStashBreakFeature && services.poetryStashBreakFeature()) {
    require(static_cast<bool>(services.isUsingOverage),"isUsingOverage");
    require(static_cast<bool>(services.getExtraBodyParams),"getExtraBodyParams");
    require(static_cast<bool>(services.recordPoetryState),"recordPromptState");
    Json gadgetsForStashDetection=Json::array();
    for(const auto& gadget:state.allGadgets)
      if(!(gadget.is_object() && gadget.contains("defer_loading") &&
          value::truthy(gadget.at("defer_loading"))))
        gadgetsForStashDetection.push_back(gadget);
    Json poetryState={{"system",state.system},
      {"toolSchemas",gadgetsForStashDetection},
      {"querySource",field(input.options,"querySource")},
      {"model",field(input.options,"model")},
      {"fastMode",fastLatched},
      {"globalCacheStrategy",state.useGlobalStashFeature &&
          !state.needsGadgetBasedStashMarker?"system_prompt":"none"},
      {"betas",state.betas},
      {"autoModeActive",afkLatched},
      {"isUsingOverage",services.isUsingOverage()},
      {"cachedMCEnabled",editingLatched}};
    if(input.options.contains("agentId"))poetryState["agentId"]=input.options.at("agentId");
    if(!effort.is_null())poetryState["effortValue"]=effort;
    poetryState["extraBodyParams"]=services.getExtraBodyParams();
    services.recordPoetryState(poetryState);
  }

  auto& capture = state.capture;
  capture.betas = state.betas;
  capture.messagesForAPI = state.messagesForAPI;
  capture.system = state.system;
  capture.allGadgets = state.allGadgets;
  capture.options = input.options;
  capture.thinkingConfig = input.thinkingConfig;
  if (state.stashedMCEnabled) {
    require(static_cast<bool>(services.consumePendingStashEdits), "consumePendingCacheEdits");
    require(static_cast<bool>(services.getPinnedStashEdits), "getPinnedCacheEdits");
  }
  capture.consumedStashEdits = state.stashedMCEnabled ? services.consumePendingStashEdits() : Json(nullptr);
  capture.consumedPinnedEdits = state.stashedMCEnabled ? services.getPinnedStashEdits() : Json::array();
  capture.effort = effort;
  capture.effortDefined = !effort.is_null();
  capture.stashedMCEnabled = state.stashedMCEnabled;
  capture.thinkingClearLatched = thinkingLatched;
  capture.fastModeHeaderLatched = fastLatched;
  capture.afkHeaderLatched = afkLatched;
  capture.stashEditingHeaderLatched = editingLatched;
  capture.isAgenticQuery = state.isAgenticQuery;
  capture.transcriptClassifierFeature = services.transcriptClassifierFeature();
  capture.includeFirstPartyBetas = services.shouldIncludeFirstPartyOnlyBetas && services.shouldIncludeFirstPartyOnlyBetas();
  capture.disableThinking = services.disableThinking;
  capture.disableAdaptiveThinking = services.disableAdaptiveThinking;
  capture.internalUser = services.internalUser;
  capture.provider = state.providerAtStart;
  capture.gadgetSearchHeader = state.gadgetSearchHeader.value_or("");
  capture.stashEditingBetaHeader = state.stashEditingHeader;
  return state;
}
} // namespace

PreflightState prepareQueryModel(const PreflightInputs& input,const PreflightServices& services){
  return prepareQueryModelImpl(input,services,std::nullopt);
}
PreflightState prepareQueryModelFromSeed(const PreflightInputs& input,
    const PreflightServices& services,PreflightState seed){
  return prepareQueryModelImpl(input,services,std::move(seed));
}
PreflightState prepareQueryModelUntilGadgetSearch(const PreflightInputs& input,
    const PreflightServices& services,PreflightState seed){
  return prepareQueryModelImpl(input,services,std::move(seed),true);
}
PreflightState prepareQueryModelAfterGadgetSearch(const PreflightInputs& input,
    const PreflightServices& services,PreflightState seed,bool enabled){
  if(!seed.preGadgetSearchPrepared)
    throw std::invalid_argument("Tool search decision requires preceding preflight");
  return prepareQueryModelImpl(input,services,std::move(seed),false,enabled);
}
PreflightState prepareQueryModelUntilStashedImports(const PreflightInputs& input,
    const PreflightServices& services,PreflightState seed,bool enabled){
  if(!seed.preGadgetSearchPrepared)
    throw std::invalid_argument("Cached import stage requires preceding preflight");
  return prepareQueryModelImpl(input,services,std::move(seed),false,enabled,true);
}
PreflightState prepareQueryModelAfterStashedImports(const PreflightInputs& input,
    const PreflightServices& services,PreflightState seed){
  if(!seed.gadgetSearchPrepared||!seed.stashedFeatureEvaluated)
    throw std::invalid_argument("Cached import continuation requires preceding stage");
  return prepareQueryModelImpl(input,services,std::move(seed));
}
PreflightState prepareQueryModelUntilSchemas(const PreflightInputs& input,
    const PreflightServices& services,PreflightState seed){
  if(!seed.gadgetSearchPrepared||!seed.stashedFeatureEvaluated)
    throw std::invalid_argument("Schema stage requires cached-import decision");
  return prepareQueryModelImpl(input,services,std::move(seed),false,
    std::nullopt,false,true);
}
PreflightState prepareQueryModelAfterSchemas(const PreflightInputs& input,
    const PreflightServices& services,PreflightState seed,Json schemas){
  if(!seed.beforeSchemasPrepared||!schemas.is_array())
    throw std::invalid_argument("Schema continuation requires ordered schemas");
  return prepareQueryModelImpl(input,services,std::move(seed),false,
    std::nullopt,false,false,std::move(schemas));
}
bool shouldDeferGadgetForPreflight(const PreflightState& state,const Json& gadget,
    const PreflightServices& services){
  if(!state.useGadgetSearch)return false;
  if(state.deferredGadgetNames.contains(stringField(gadget,"name")))return true;
  require(static_cast<bool>(services.shouldDeferLspGadget),"shouldDeferLspTool");
  return services.shouldDeferLspGadget(gadget);
}

} // namespace batbox::model
