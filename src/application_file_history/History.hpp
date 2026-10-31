#pragma once
#include "app_state/State.hpp"
#include "application_flights/Flight.hpp"
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace batbox::application_file_history {
struct DiffStats {
  std::vector<std::string> filesChanged;
  std::size_t insertions=0,deletions=0;
};
struct Inputs {
  event_loop::Loop& owner;
  std::shared_ptr<app_state::AppStateStore> store;
  std::shared_ptr<application_flights::Flight> flight;
  std::filesystem::path configHome,originalCwd;
  bool nonInteractive=false;
  std::function<bool()> configuredEnabled;
  std::function<void(std::exception_ptr)> logError;
  std::function<void(const std::filesystem::path&,
    const std::optional<std::string>&,
    const std::optional<std::string>&)> notifyEditorFileUpdated;
  explicit Inputs(event_loop::Loop& loop):owner(loop){}
};

// FileHistoryState is kept in AppState as JSON. Backup files use
// hash@version names under configHome/file-history/flightId.
class History final:public std::enable_shared_from_this<History> {
public:
  static std::shared_ptr<History> create(Inputs);
  bool enabled()const;
  Json state()const;
  void trackEdit(const std::string& filePath,const std::string& messageId);
  event_loop::Deferred<event_loop::Unit> makeSnapshot(const std::string& messageId);
  event_loop::Deferred<event_loop::Unit> rewind(const std::string& messageId);
  bool canRestore(const std::string& messageId)const;
  event_loop::Deferred<std::optional<DiffStats>> getDiffStats(
    const std::string& messageId);
  event_loop::Deferred<bool> hasAnyChanges(const std::string& messageId);
  void restoreStateFromLog(const Json& snapshots);
  event_loop::Deferred<event_loop::Unit> copyForResume(const Json& log);
  std::filesystem::path backupPath(const std::string& name,
    std::optional<std::string> flightId={})const;
private:
  explicit History(Inputs inputs):inputs_(std::move(inputs)){}
  void setState(Json value);
  void record(const std::string& messageId,const Json& snapshot,bool update);
  void report(std::exception_ptr error)const;
  std::string trackingPath(const std::filesystem::path& file)const;
  std::filesystem::path expandPath(const std::string& path)const;
  Json createBackup(const std::filesystem::path& file,std::size_t version,
    const std::string& flightId);
  bool changed(const std::filesystem::path& file,const std::string& backup,
    const std::string& flightId)const;
  bool restoreBackup(const std::filesystem::path& file,const std::string& backup,
    const std::string& flightId)const;
  std::optional<std::optional<std::string>> backupAt(
    const Json& state,const Json& snapshot,const std::string& path)const;
  std::optional<Json> findSnapshot(const Json& state,const std::string& id)const;
  std::vector<std::string> applySnapshot(const Json& state,const Json& target,
    const std::string& flightId);
  DiffStats diffStats(const Json& state,const Json& target,
    const std::string& flightId)const;
  Inputs inputs_;
};
} // namespace batbox::application_file_history
