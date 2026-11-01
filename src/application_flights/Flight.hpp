#pragma once
#include "application/Config.hpp"
#include "application_flights/Plans.hpp"
#include "application_roost/Roost.hpp"
#include "flights/AsyncStash.hpp"
#include "flights/AsyncLoad.hpp"
#include "flights/ProjectDirectories.hpp"
#include "flights/async_project/AsyncProject.hpp"

namespace batbox::application_flights {
struct Inputs {
  event_loop::Loop& owner;
  application::Config config;
  std::shared_ptr<application_roost::Roost> roost;
  std::string flightId;
  std::function<void(std::exception_ptr)> logError;
  std::function<std::set<std::string>()> commandNames;
  explicit Inputs(event_loop::Loop& loop):owner(loop){}
};
class Flight final:public std::enable_shared_from_this<Flight> {
public:
  static std::shared_ptr<Flight> create(Inputs inputs);
  std::shared_ptr<flights::async_project::AsyncProject> project()const{return project_;}
  std::shared_ptr<flights::Paths> paths()const{return paths_;}
  event_loop::Deferred<flights::LoadedTranscript> load(const std::string& flight);
  event_loop::Deferred<bool> doesMessageExistInFlight(const std::string& uuid);
  event_loop::Deferred<std::optional<std::string>> record(
    const std::vector<flights::MessageRef>& messages);
  event_loop::Deferred<event_loop::Unit> recordBat(const std::string& bat,
    const std::vector<flights::MessageRef>& messages,
    std::optional<std::string> parent={});
  event_loop::Deferred<event_loop::Unit> flush();
  event_loop::Deferred<event_loop::Unit> resetToNewId(std::string id);
  event_loop::Deferred<event_loop::Unit> adoptResumedId(std::string id,
    std::optional<std::string> projectDirectory={});
  void reappendMetadata();
  // Clean shutdown: re-append titles and other metadata near the end of the
  // transcript, then write a flight-end marker. Call after flush(); a Flight
  // that never wrote a message has no file and is left alone.
  void markEnded();
  void clearMessageStash();
  void recordReplacement(const Json& replacements,const Json& bat);
  const std::string& id()const{return *flightId_;}
  std::filesystem::path workingDirectory()const;
  std::filesystem::path planFilePath(const std::optional<std::string>& bat={});
  std::optional<std::string> planContent(const std::optional<std::string>& bat={});
  bool restorePlan(const Json& log,bool remote=false);
  bool copyPlanForFork(const Json& log,const std::string& targetFlight);
private:
  explicit Flight(Inputs inputs):inputs_(std::move(inputs)){}
  void initialize();
  Inputs inputs_;
  std::shared_ptr<std::string> flightId_;
  std::shared_ptr<std::optional<std::string>> projectDirectory_;
  std::shared_ptr<flights::ProjectDirectories> directories_;
  std::shared_ptr<flights::Paths> paths_;
  std::shared_ptr<flights::AsyncFlightMessageStash> stash_;
  std::shared_ptr<flights::FlightState> state_;
  std::shared_ptr<flights::async_queue::Queue> queue_;
  std::shared_ptr<flights::async_project::AsyncProject> project_;
  std::shared_ptr<PlanFiles> plans_;
  flights::AsyncLoadServices loadServices_;
};
} // namespace batbox::application_flights
