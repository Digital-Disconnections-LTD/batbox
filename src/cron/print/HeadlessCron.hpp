#pragma once
#include "cron/use_scheduled/Flight.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace batbox::cron::print {
struct Services {
  std::function<bool()> batTriggersBuilt;
  bool schedulerModuleAvailable=false;
  std::function<bool()> cronEnabled;
  std::function<CronJitterConfig()> getJitterConfig;
  std::function<use_scheduled::SchedulerHandle(SchedulerOptions)>
    createScheduler;
  std::function<void(const Json&)> enqueue;
  std::function<std::string()> randomUUID;
  std::function<void()> run;
};
class HeadlessCron {
public:
  explicit HeadlessCron(Services services);
  void start();
  void setRunning(bool running){state_->running=running;}
  void closeInput();
  bool inputClosed()const{return state_->inputClosed;}
private:
  struct State {bool inputClosed=false,running=false;};
  std::shared_ptr<Services> services_;
  std::shared_ptr<State> state_=std::make_shared<State>();
  std::optional<use_scheduled::SchedulerHandle> scheduler_;
  bool started_=false;
};
}
