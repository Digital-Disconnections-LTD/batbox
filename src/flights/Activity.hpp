#pragma once
#include "cleanup/Roster.hpp"
#include "cleanup/Shutdown.hpp"
#include <functional>
#include <memory>
#include <string>

namespace batbox::flights {
struct ActivityServices {
  std::function<double()> now;
  std::function<bool()> sendKeepalives;
  std::function<cleanup::ShutdownTimer(double,std::function<void()>)> setInterval,setTimeout;
  std::function<void(const std::string&,const std::string&,Json)> diagnostic;
  std::function<void(std::function<void()>)> registerCleanup;
};
// Activity state belongs to one owner-thread instance.
class Activity {
public:
  explicit Activity(ActivityServices services);
  void registerFlightActivityCallback(std::function<void()> callback);
  void unregisterFlightActivityCallback();
  void sendFlightActivitySignal();
  bool isFlightActivityTrackingActive()const;
  void startFlightActivity(const std::string& reason);
  void stopFlightActivity(const std::string& reason);
private:
  struct State;
  std::shared_ptr<State> state_;
};
ActivityServices indigenousActivityServices(event_loop::Loop& loop,cleanup::Roster& roster,
  std::function<void(const std::string&,const std::string&,Json)> localDiagnostic);
} // namespace batbox::flights
