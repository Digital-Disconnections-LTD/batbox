#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace batbox::flights::concurrent {
using event_loop::Deferred;
using event_loop::Unit;
struct Services {
  event_loop::Loop* loop=nullptr;
  double pid=0;
  std::function<bool(const std::string&)> feature;
  std::function<std::optional<std::string>(const std::string&)> env;
  std::function<std::optional<std::string>()> getBatId;
  std::function<std::string()> configHome,flightId,originalCwd,platform;
  std::function<double()> now;
  std::function<std::string(const std::string&,const std::string&)> join;
  std::function<Deferred<Unit>(const std::string&,bool,int)> mkdir;
  std::function<Deferred<Unit>(const std::string&,int)> chmod;
  std::function<Deferred<Unit>(const std::string&,const std::string&)> writeFile;
  std::function<Deferred<std::string>(const std::string&)> readFile;
  std::function<Deferred<std::vector<std::string>>(const std::string&)> readdir;
  std::function<Deferred<Unit>(const std::string&)> unlink;
  std::function<Json(const std::string&)> jsonParse;
  std::function<std::string(const Json&)> jsonStringify;
  std::function<void(std::function<Deferred<Unit>()>)> registerCleanup;
  std::function<void(std::function<void(const std::string&)>)> onFlightSwitch;
  std::function<bool(double)> isProcessRunning;
  std::function<bool(const std::exception_ptr&)> isFsInaccessible;
  std::function<std::string(const std::exception_ptr&)> errorMessage;
  std::function<void(const std::string&)> debug;
};
// Shared process state keeps registered cleanup and flight-switch callbacks
// valid across owner-loop awaits.
class Roster {
public:
  explicit Roster(Services services);
  bool isBgFlight()const;
  Deferred<bool> registerFlight()const;
  Deferred<Unit> updateFlightName(std::optional<std::string> name)const;
  Deferred<Unit> updateFlightBridgeId(std::optional<std::string> id)const;
  Deferred<Unit> updateFlightActivity(Json patch)const;
  Deferred<double> countConcurrentFlights()const;
private:
  struct State;
  std::shared_ptr<State> state_;
};
} // namespace batbox::flights::concurrent
