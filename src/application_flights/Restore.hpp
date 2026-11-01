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
  // Plan/auto mode bookkeeping when a resumed Flight changes permission mode.
  std::function<void(const std::string& from,const std::string& to)>
    transitionPermissionMode;
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
  // Resolves --resume <value>: a Flight id, or a slug, title or search words
  // matched against this directory, its worktrees, then all projects.
  // Rejects with a message listing candidates when the value is ambiguous.
  event_loop::Deferred<std::optional<Json>> find(const std::string& query);
private:
  explicit Restore(RestoreInputs inputs):inputs_(std::move(inputs)){}
  event_loop::Deferred<std::vector<std::string>> worktreePaths(const std::string& cwd);
  event_loop::Deferred<std::optional<Json>> loadPath(
    const std::filesystem::path& path,const std::string& id);
  RestoreInputs inputs_;
};
// Model and permission mode a Flight last ran with, read from its messages.
struct RecordedLaunchChoices {
  std::optional<std::string> model,permissionMode;
};
RecordedLaunchChoices recordedLaunchChoices(const Json& log);
// Profile id serving a recorded model (matched by id or wire model).
std::optional<std::string> profileForModel(const application::Config& config,
  const std::string& model);
// Permission modes a resume may restore by itself. bypassPermissions is never
// restored silently; it needs an explicit --yolo or /permissions.
bool restorablePermissionMode(const std::string& mode);
// Walks records (lite logs, newest first) and resolves to the first one the
// loader turns into a full log. A record that fails to load is skipped; the
// first failure is only reported when nothing at all could be loaded.
event_loop::Deferred<std::optional<Json>> firstLoadable(event_loop::Loop& owner,
  Json records,std::function<event_loop::Deferred<std::optional<Json>>(const Json&)> loader);
} // namespace batbox::application_flights
