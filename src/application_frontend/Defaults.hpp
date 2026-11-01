#pragma once
#include "application_runtime/ApplicationRuntime.hpp"
#include "application_poetry/Provider.hpp"
#include "application_flights/Flight.hpp"
#include "application_permissions/PermissionProvider.hpp"
#include "messages/RuntimeMessages.hpp"
#include "user_input/BashInput.hpp"
#include "user_input/BashIndigenousBinding.hpp"
#include "application_media/Bindings.hpp"
#include "application_history/History.hpp"
#include "interactive/navigation/Rewind.hpp"
#include "application_file_history/History.hpp"
#include <stop_token>

namespace batbox::application_frontend {
struct IndigenousBashCommandInputs {
  event_loop::Loop& owner;
  messages::Services& messages;
  gadget_results::StorageImports storage;
  std::function<gadgets::Gadget()> getBashGadget;
  std::function<gadgets::GadgetCallContext()> getContext;
  std::function<void(const std::string&,const std::optional<Json>&)> showProgress;
  std::function<void()> clearProgress;
  IndigenousBashCommandInputs(event_loop::Loop& loop,messages::Services& service):
    owner(loop),messages(service){}
};
std::function<event_loop::Deferred<user_input::InputResult>(
  const std::string&,const Json&,const Json&)>
makeIndigenousBashCommand(IndigenousBashCommandInputs inputs);
// Callback services for the standard local GUI path. Optional callbacks
// select extra integrations; enabled features without a provider fail clearly.
struct Inputs {
  messages::Services* messages=nullptr;
  std::shared_ptr<application_poetry::Provider> poetry;
  std::shared_ptr<application_flights::Flight> flight;
  std::shared_ptr<application_permissions::PermissionProvider> permission;
  std::shared_ptr<application_media::Bundle> media;
  std::shared_ptr<application_history::Bundle> history;
  std::shared_ptr<application_file_history::History> fileHistory;
  bool persistOnTurnComplete=true;
  std::function<bool(const std::string&)> feature;
  std::function<void(const std::string&)> log;
  std::function<void(const std::string&)> showImmediateNotice;
  std::function<void(std::exception_ptr)> onError;
  std::function<void()> redraw,shutdown;
  std::function<std::shared_ptr<application_runtime::ApplicationRuntime>()> getRuntime;
  std::function<std::shared_ptr<interactive::conversation_shell::Shell>()> getShell;
  std::function<void()> resetMicrocompactState,resetContextCollapse;
  std::function<event_loop::Deferred<bool>(value::ValueRef,const std::string&)>
    fileHistoryHasAnyChanges;
  std::function<event_loop::Deferred<Json>(const std::string&,const Json&,
    const Json&,const Json&,const std::optional<std::string>&)> attachments;
  std::function<event_loop::Deferred<Json>(const Json&)> resizeImage,storeImages;
  std::function<event_loop::Deferred<user_input::InputResult>(
    const std::string&,const Json&,const Json&)> bashCommand;
  std::function<instructions::hooks::runtime::HookEventGenerator(
    const std::string&,const std::string&,std::stop_token)> userPoetryHooks;
  std::function<event_loop::Deferred<event_loop::Unit>(const std::string&,
    const interactive::TurnMessagesRef&,std::size_t)> beforeQuery;
  std::function<void(const Json&)> diagnosticQueryStart;
  std::function<void(const std::string&)> setFlightTitle;
  // Short AI title for a new Flight from its first prompt. It resolves to
  // nullopt when no title should or could be generated; it never rejects.
  std::function<event_loop::Deferred<std::optional<std::string>>(
    const std::string&)> generateFlightTitle;
  std::function<std::shared_ptr<interactive::TurnAbort>()> activeAbort;
  std::function<bool()> hasInterruptibleGadgetInProgress;
  std::function<void(const interactive::InteractiveQueryContext&)> onQueryContextCreated;
  std::function<void(const interactive::TurnMessagesRef&)> launchMonsterObserver;
  Json initialMcpClients=Json::array();
};
class Bundle final:public std::enable_shared_from_this<Bundle> {
public:
  static std::shared_ptr<Bundle> bind(application_runtime::Inputs& runtime,Inputs inputs);
  void rebindEnvironment(application::Config config,
    std::shared_ptr<application_poetry::Provider> poetry,
    std::vector<user_input::CommandEntry> commands,
    Json initialMcpClients=Json(),
    std::shared_ptr<application_file_history::History> fileHistory={});
  event_loop::Deferred<event_loop::Unit> editMessage(interactive::MessageRef message);
  void openRewind();
private:
  Bundle(application_runtime::Inputs& runtime,Inputs inputs);
  void install(application_runtime::Inputs& runtime);
  std::shared_ptr<user_input::UserInputServices> makeUserInputServices();
  interactive::submit::PoetrySubmitRequest submitRequest(const std::string& text,
    const interactive::InputState& input)const;
  interactive::QueryExecutionParams queryParams()const;
  void initializeRewind();
  void showRewindSelector();
  event_loop::Loop& owner_;
  application::Config config_;
  interactive::PresentationState& state_;
  std::shared_ptr<app_state::AppStateStore> store_;
  Inputs inputs_;
  std::vector<user_input::CommandEntry> commands_;
  std::size_t submitCount_=0;
  double lastInteractionMs_=0;
  std::shared_ptr<interactive::navigation::Rewind> rewind_;
};
bool selectableUserMessage(const interactive::TurnMessageRef& message);
bool messagesAfterAreOnlySynthetic(const interactive::TurnMessagesRef& messages,
  std::size_t index);
} // namespace batbox::application_frontend
