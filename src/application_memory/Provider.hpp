#pragma once
#include "application/Config.hpp"
#include "application_model/LocalModelService.hpp"
#include "application_poetry/Provider.hpp"
#include "application_flights/Flight.hpp"
#include "bats/ForkedBat.hpp"
#include "memory_prefetch/Prefetch.hpp"
#include <optional>

namespace batbox::application_engine {class Engine;}
namespace batbox::application_memory {
struct Inputs {
  event_loop::Loop& owner;
  application::Config config;
  std::shared_ptr<application_poetry::Provider> poetry;
  std::shared_ptr<application_model::LocalModelService> model;
  std::shared_ptr<application_flights::Flight> flight;
  std::shared_ptr<messages::Services> messages;
  std::function<Json()> getAppState;
  std::function<void(const std::function<Json(const Json&)>&)> updateAppState;
  std::function<std::shared_ptr<bats::BatContext>()> activeBatContext;
  std::function<std::shared_ptr<application_engine::Engine>()> currentEngine;
  std::function<void(const std::string&)> log;
  std::function<void(const Json&)> notify;
  explicit Inputs(event_loop::Loop& loop):owner(loop){}
};

// One owner per roost generation. All public calls require its owner loop.
// Files remain the durable source of truth; no separate memory database.
class Provider final:public std::enable_shared_from_this<Provider> {
public:
  static std::shared_ptr<Provider> create(Inputs inputs);
  std::shared_ptr<memory_prefetch::Services> makePrefetchServices();
  event_loop::Deferred<Json> listSources();
  event_loop::Deferred<std::string> read(const std::string& path);
  event_loop::Deferred<event_loop::Unit> prepareForEdit(const std::string& path);
  event_loop::Deferred<event_loop::Unit> write(const std::string& path,
    const std::string& content);
  event_loop::Deferred<Json> status();
  std::string autoMemoryDirectory();
  bool hasActiveJobs()const;
  void refresh();
  // Bind after successful file-gadget changes and FileChanged observations.
  // Bash/unknown-path writes call observeFileChange("") to inspect index stamps.
  void observeFileChange(const std::string& path);
  void executeAutoDream(const Json& stopContext);
  event_loop::Deferred<event_loop::Unit> consolidate(const Json& stopContext);
  event_loop::Deferred<event_loop::Unit> shutdown();
private:
  explicit Provider(Inputs inputs):inputs_(std::move(inputs)){}
  void requireOwner()const;
  Json settings()const;
  instructions::MemoryPathServices paths();
  bool recallEnabled()const;
  bool dreamEnabled()const;
  std::vector<std::pair<std::string,std::string>> memoryDirectories();
  event_loop::Deferred<event_loop::Unit> launchDream(const Json&,
    bool manual);
  Json runFork(const std::shared_ptr<bats::StashSafeParams>& stash,
    const std::string& poetry,const std::string& source,
    std::stop_token signal,const std::function<void(const Json&)>& progress,
    std::optional<int> maxTurns={});
  void updateMission(const std::string& id,const std::function<Json(Json)>& update);
  Inputs inputs_;
  instructions::MemoryPathStash pathStash_;
  std::shared_ptr<memory_prefetch::Services> prefetch_;
  std::optional<event_loop::Deferred<event_loop::Unit>> activeDream_;
  std::shared_ptr<std::stop_source> dreamStop_;
  double lastFlightScanAt_=0;
  std::string lastTurnUuid_,lastResult_="Not run";
  bool closing_=false;
  std::size_t recallJobs_=0;
  std::vector<std::stop_source> recallStops_;
  std::vector<event_loop::Deferred<event_loop::Unit>> idleWaiters_;
};
} // namespace batbox::application_memory
