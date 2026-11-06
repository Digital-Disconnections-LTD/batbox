#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <filesystem>
#include <functional>
#include <set>
#include <string>

namespace batbox::application {
// Each running Batbox names the Flight it holds in <state>/running/<pid>, so
// retention in another process leaves that Flight alone.
void recordRunningFlight(const std::filesystem::path& stateDirectory,const std::string& flightId);
void forgetRunningFlight(const std::filesystem::path& stateDirectory);
// Flights named by live processes; files of processes that ended are removed.
std::set<std::string> heldFlights(const std::filesystem::path& stateDirectory);
struct RetentionInputs {
  std::filesystem::path stateDirectory;
  // The launch settings: cleanupPeriodDays (default 30) and their validity.
  Json settings=Json::object();
  std::function<std::string()> currentFlight;
  std::function<event_loop::Deferred<event_loop::Unit>(double cutoffMs)> oldPastes;
  std::function<event_loop::Deferred<event_loop::Unit>(double cutoffMs,
    std::set<std::string> held)> oldImageStashes;
  std::function<void(const std::string&)> log;
};
// Deletes what is older than cleanupPeriodDays under the state directory:
// Flight transcripts and their Gadget results, plans, debug logs, shell
// output, file-history backups, Flight environment folders, pastes and image
// stashes. The current Flight and any Flight a running process holds are
// never touched, whatever their age.
event_loop::Deferred<event_loop::Unit> runRetention(event_loop::Loop& owner,
  RetentionInputs inputs);
}
