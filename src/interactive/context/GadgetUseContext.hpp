#pragma once

#include "app_state/OnChange.hpp"
#include "file_state_stash/FileStateStash.hpp"
#include "interactive/CommandUi.hpp"
#include "interactive/QueryTurn.hpp"
#include "runtime/OrderedStringSet.hpp"
#include "gadget_results/State.hpp"
#include "gadgets/Gadget.hpp"

namespace batbox::interactive::context {
using StateUpdater=app_state::AppStateUpdater;
using ValueRef=value::ValueRef;
using ValueUpdater=std::function<ValueRef(const ValueRef&)>;

struct ProcessOptions {
  ValueRef commands;
  gadgets::Gadgets gadgets;
  bool debug=false,verbose=false,isNonInteractiveFlight=false;
  std::string mainLoopModel;
  ValueRef thinkingConfig,mcpClients;
  ValueRef mcpResources,batDefinitions;
  Json ideInstallationStatus,theme,dynamicMcpConfig;
  std::optional<std::string> customSystemPoetry,appendSystemPoetry;
  std::function<gadgets::Gadgets()> refreshGadgets;
};

struct GadgetUseContext {
  std::shared_ptr<TurnAbort> abortController;
  ProcessOptions options;
  std::shared_ptr<app_state::AppStateStore> store;
  TurnMessagesRef messages;
  std::function<void(const TurnMessageUpdate&)> setMessages;
  std::function<app_state::AppStateRef()> getAppState;
  std::function<void(const StateUpdater&)> setAppState;
  std::function<void(const ValueUpdater&)> updateFileHistoryState,
    updateAttributionState;
  std::function<void()> openMessageSelector,onChangeAPIKey;
  std::function<event_loop::Deferred<event_loop::Unit>(
    const std::string&,const Json&,const std::string&)> resume;
  std::shared_ptr<file_state_stash::FileStateStash> readFileState;
  std::function<void(std::optional<UiRequest>)> setLocalUi;
  std::function<void(const Json&)> addNotification,sendOSNotification,
    onChangeDynamicMcpConfig;
  std::function<void(const std::function<double(double)>&)> setResponseLength;
  using SetRef=std::shared_ptr<runtime::OrderedStringSet>;
  std::function<void(const std::function<SetRef(const SetRef&)>&)>
    setInProgressGadgetUseIDs;
  std::function<void(const std::string&)> onInstallIDEExtension,setStreamMode,
    setConversationId;
  std::function<void(const TurnMessageRef&)> appendSystemMessage;
  std::shared_ptr<runtime::OrderedStringSet> nestedMemoryAttachmentTriggers,
    loadedNestedMemoryPaths,dynamicSkillDirTriggers,discoveredSkillNames;
  std::function<void(const Json&)> onCompactProgress;
  std::function<void(bool)> setHasInterruptibleGadgetInProgress;
  std::function<std::function<event_loop::Deferred<Json>(const Json&)>(
    const std::string&,const std::optional<std::string>&)> requestPoetry;
  std::shared_ptr<gadget_results::ContentReplacementState> contentReplacementState;
  std::shared_ptr<const void> gadgetSnapshotOwner;
};
} // namespace batbox::interactive::context
