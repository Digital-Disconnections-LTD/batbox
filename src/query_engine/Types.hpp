#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/OrderedStringSet.hpp"
#include "runtime/SharedRecord.hpp"
#include "gadgets/Gadget.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace batbox::file_state_stash {class FileStateStash;}
namespace batbox::query_engine {
using MessageRef=std::shared_ptr<Json>;
using Messages=std::vector<MessageRef>;
using MessagesRef=std::shared_ptr<Messages>;
using JsonRef=std::shared_ptr<Json>;
using AppStateRef=value::RecordRef;
using AppStateUpdater=std::function<AppStateRef(const AppStateRef&)>;
using SetAppState=std::function<void(const AppStateUpdater&)>;
using ValueUpdater=std::function<JsonRef(const JsonRef&)>;
using SetValue=std::function<void(const ValueUpdater&)>;
using GadgetsRef=std::shared_ptr<gadgets::Gadgets>;
using FileStateStash=::batbox::file_state_stash::FileStateStash;
using FileStateStashRef=std::shared_ptr<FileStateStash>;
struct AbortController {
  virtual ~AbortController()=default;
  virtual void abort()=0;
};
using AbortControllerRef=std::shared_ptr<AbortController>;
struct ProcessContext;
using ProcessContextRef=std::shared_ptr<ProcessContext>;
struct PermissionCall {
  const gadgets::Gadget* gadget=nullptr;
  JsonRef input;
  ProcessContextRef gadgetUseContext;
  MessageRef assistantMessage;
  std::string gadgetUseID;
  Json forceDecision=Json(Json::value_t::discarded);
};
using CanUseGadget=std::function<event_loop::Deferred<JsonRef>(const PermissionCall&)>;
using HandleElicitation=std::function<event_loop::Deferred<Json>(const Json&)>;
struct SnipResult {MessagesRef messages;bool executed=false;};
using SnipReplay=std::function<std::optional<SnipResult>(const MessageRef&,const MessagesRef&)>;
struct Configuration {
  MessagesRef initialMessages;
  AbortControllerRef abortController;
  FileStateStashRef readFileStash;
  std::optional<std::string> userSpecifiedModel;
  std::string cwd;
  GadgetsRef gadgets;
  JsonRef commands,mcpClients,bats;
  CanUseGadget canUseGadget;
  std::function<AppStateRef()> getAppState;
  SetAppState setAppState;
  std::optional<std::string> customSystemPoetry,appendSystemPoetry,fallbackModel;
  JsonRef thinkingConfig,missionBudget,jsonSchema;
  std::optional<double> maxTurns,maxBudgetUsd;
  bool verbose=false,replayUserMessages=false,includePartialMessages=false;
  HandleElicitation handleElicitation;
  std::function<void(const Json&)> setSDKStatus;
  JsonRef orphanedPermission;
  SnipReplay snipReplay;
};
struct ProcessOptions {
  JsonRef commands,mcpClients,activeBats,allBats;
  GadgetsRef gadgets;
  bool debug=false,verbose=false,isNonInteractiveFlight=true;
  std::string mainLoopModel;
  JsonRef thinkingConfig;
  Json mcpResources=Json::object(),ideInstallationStatus=nullptr,theme;
  std::optional<std::string> customSystemPoetry,appendSystemPoetry;
  std::optional<double> maxBudgetUsd;
};
struct ProcessContext {
  MessagesRef messages;
  std::function<void(const std::function<MessagesRef(const MessagesRef&)>&)> setMessages;
  std::function<void()> onChangeAPIKey;
  HandleElicitation handleElicitation;
  ProcessOptions options;
  std::function<AppStateRef()> getAppState;
  SetAppState setAppState;
  AbortControllerRef abortController;
  FileStateStashRef readFileState;
  std::shared_ptr<runtime::OrderedStringSet> nestedMemoryAttachmentTriggers,
    loadedNestedMemoryPaths,dynamicSkillDirTriggers,discoveredSkillNames;
  std::function<void(const Json&)> setInProgressGadgetUseIDs,setResponseLength;
  SetValue updateFileHistoryState,updateAttributionState;
  std::function<void(const Json&)> setSDKStatus;
};
struct SubmitOptions {std::optional<std::string> uuid;std::optional<bool> isMeta;};
struct UserInputCall {
  Json input;
  std::string mode="prompt",querySource="sdk";
  std::function<void(const Json&)> setGadgetJSX;
  ProcessContextRef context;
  MessagesRef messages;
  SubmitOptions options;
};
struct UserInputResult {
  MessagesRef messages;
  bool shouldQuery=false;
  JsonRef allowedGadgets;
  std::optional<std::string> model,resultText;
};
struct QueryCall {
  MessagesRef messages;
  Json systemPoetry,userContext,systemContext;
  CanUseGadget canUseGadget;
  ProcessContextRef gadgetUseContext;
  std::optional<std::string> fallbackModel;
  std::string querySource="sdk";
  std::optional<double> maxTurns;
  JsonRef missionBudget;
};
// Imported generators remain pull-based so effects after a yield occur only
// when the consumer resumes. Both cursors execute on the owner event loop.
struct MessageCursor {
  std::function<event_loop::Deferred<std::optional<MessageRef>>()> next;
  std::function<event_loop::Deferred<event_loop::Unit>()> close;
};
struct SdkCursor {std::function<std::optional<Json>()> next;};
}
