#pragma once
#include "application/Config.hpp"
#include "application_flights/Flight.hpp"
#include "application_roost/Roost.hpp"
#include "app_state/State.hpp"
#include "commands_indigenous/UiCommands.hpp"
#include "event_loop/Deferred.hpp"
#include "file_state_stash/FileStateStash.hpp"
#include "messages/Messages.hpp"
#include "gadget_results/State.hpp"
#include <filesystem>
#include <memory>

namespace batbox::application_flights {
struct RestoreInputs {
  event_loop::Loop& owner;
  application::Config config;
  std::shared_ptr<Flight> flight;
  std::shared_ptr<application_roost::Roost> roost;
  application_roost::Roost::Rebind rebindRoost;
  std::shared_ptr<app_state::AppStateStore> store;
  std::shared_ptr<file_state_stash::FileStateStash> fileStash;
  std::shared_ptr<messages::Services> messages;
  std::shared_ptr<gadget_results::ContentReplacementState> replacementState;
  std::function<void(const Json&)> setMessages;
  std::function<void(const std::string&)> setConversationId;
  std::function<void(std::shared_ptr<gadget_results::ContentReplacementState>)>
    setReplacementState;
  std::function<event_loop::Deferred<event_loop::Unit>(const std::string&)>
    endFlight;
  std::function<event_loop::Deferred<Json>(const std::string&)> startFlight;
  // Optional state/presentation notification after Roost has switched;
  // it must not change cwd or rebind providers itself.
  std::function<void(const Json&)> onWorktreeRestored;
  // Invoked after AppState restoration so the indigenous history owner can
  // migrate absolute tracking keys and rebuild its tracked-file set.
  std::function<void(const Json&)> onFileHistoryRestored;
  std::function<event_loop::Deferred<std::optional<std::string>>(
    const std::string&)> copyToClipboard;
  std::function<void(const std::string&)> writeOutput;
  std::function<bool()> canResume;
  std::function<bool()> fileHistoryEnabled;
  std::function<void(std::exception_ptr)> logError;
  explicit RestoreInputs(event_loop::Loop& loop):owner(loop){}
};

// Full transcript recovery for --resume, --continue and the /resume picker.
// load() walks the persisted parentUuid graph through the indigenous flight loader.
class Restore final:public std::enable_shared_from_this<Restore> {
public:
  static std::shared_ptr<Restore> create(RestoreInputs);
  event_loop::Deferred<std::optional<Json>> load(const std::string& id);
  event_loop::Deferred<std::optional<Json>> latest();
  event_loop::Deferred<Json> list(const std::vector<std::string>& projectPaths,
    bool allProjects=false);
  event_loop::Deferred<event_loop::Unit> resume(const std::string& id,
    const Json& log,const std::string& entrypoint);
  std::shared_ptr<commands_indigenous::ResumeServices> makeResumeServices();
private:
  explicit Restore(RestoreInputs inputs):inputs_(std::move(inputs)){}
  event_loop::Deferred<std::optional<Json>> loadPath(
    const std::filesystem::path& path,const std::string& id);
  RestoreInputs inputs_;
};
} // namespace batbox::application_flights
