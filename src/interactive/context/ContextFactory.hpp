#pragma once

#include "bats/BatGadgets.hpp"
#include "interactive/QueryExecution.hpp"
#include "interactive/context/GadgetPool.hpp"
#include "interactive/context/GadgetUseContext.hpp"
#include "gadgets/Catalog.hpp"

namespace batbox::interactive::context {
struct SpinnerBindings {
  std::function<void(std::optional<std::string>)> setColor,setShimmerColor,
    setMessage;
  std::string primaryColor="batboxBlue",shimmerColor="batboxBlueShimmer";
};
struct ContextBindings {
  std::shared_ptr<app_state::AppStateStore> store;
  gadgets::CatalogImports catalog;
  gadgets::CatalogSettings catalogSettings;
  std::function<gadgets::Gadgets(const Json&,const gadgets::Gadgets&)> assembleGadgets;
  std::function<std::shared_ptr<const void>()> gadgetSnapshotOwner;
  bats::BatGadgetFilterImports batFilter;
  GadgetPoolServices gadgetPool;
  gadgets::Gadgets combinedInitialGadgets;
  std::function<gadgets::Gadgets(const app_state::AppStateRef&)> getMcpGadgets;
  std::optional<Json> mainThreadBatDefinition,allowedBatTypes;
  ValueRef commands=std::make_shared<Json>(Json::array()),
    initialMcpClients=std::make_shared<Json>(Json::array()),
    thinkingConfig=std::make_shared<Json>(Json::object());
  std::shared_ptr<bool> explicitThinkingOverride;
  Json ideInstallationStatus,dynamicMcpConfig,theme;
  bool debug=false,disabled=false;
  std::optional<std::string> customSystemPoetry,appendSystemPoetry;
  std::function<void(const TurnMessageUpdate&)> setMessages;
  // Compatibility boundary for older JSON-only bat imports. The application
  // must bind a coherent update of AppState fields and typed sidecars.
  std::function<void(const std::function<Json(const Json&)>&)> legacySetAppState;
  std::function<void()> showMessageSelector,onChangeAPIKey;
  std::function<event_loop::Deferred<event_loop::Unit>(
    const std::string&,const Json&,const std::string&)> resume;
  std::function<void(std::optional<UiRequest>)> setLocalUi;
  std::function<void(const Json&)> addNotification,sendOSNotification,
    onChangeDynamicMcpConfig;
  std::function<void(const std::function<double(double)>&)> setResponseLength;
  std::function<void(const std::function<GadgetUseContext::SetRef(
    const GadgetUseContext::SetRef&)>&)> setInProgressGadgetUseIDs;
  std::function<void(const std::string&)> onInstallIDEExtension,setStreamMode,
    setConversationId;
  std::function<void(bool)> setHasInterruptibleGadgetInProgress;
  std::function<std::function<event_loop::Deferred<Json>(const Json&)>(
    const std::string&,const std::optional<std::string>&)> requestPoetry;
  std::function<bool(const std::string&)> feature;
  SpinnerBindings spinner;
  std::shared_ptr<file_state_stash::FileStateStash> readFileState;
  std::shared_ptr<runtime::OrderedStringSet> loadedNestedMemoryPaths,
    discoveredSkillNames;
  std::shared_ptr<gadget_results::ContentReplacementState> contentReplacementState;
};

class ContextFactory {
public:
  explicit ContextFactory(ContextBindings bindings);
  std::shared_ptr<GadgetUseContext> make(TurnMessagesRef messages,
    TurnMessagesRef newMessages,std::shared_ptr<TurnAbort> abort,
    std::string mainLoopModel)const;
  InteractiveQueryContext makeQueryContext(const TurnMessagesRef& messages,
    const TurnRequest& request)const;
private:
  std::shared_ptr<const ContextBindings> bindings_;
  static gadgets::Gadgets computeGadgets(const ContextBindings& bindings);
};
} // namespace batbox::interactive::context
