#pragma once
#include "bootstrap/BootstrapState.hpp"
#include "cleanup/Roster.hpp"
#include "cron/CronMissions.hpp"
#include "cron/CronScheduler.hpp"
#include "cron/config/CronConfiguration.hpp"
#include "cron/print/HeadlessCron.hpp"
#include "cron/use_scheduled/Flight.hpp"
#include "gadgets/Gadget.hpp"
#include "interactive/QueryGuard.hpp"
#include "messages/Messages.hpp"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace batbox::application {
// Scheduled prompts for one process. Session jobs live in state; durable jobs
// live in <Roost>/.batbox/scheduled_tasks.json, and only when cronDurable was
// on at launch, so with it off nothing is read from or written to the Roost.
struct CronWiring {
  std::shared_ptr<bootstrap::BootstrapState> state;
  std::shared_ptr<cleanup::Roster> cleanup;
  cron::config::Services gates;
  bool durable=false;
  cron::CronMissionServices missions;
  cron::SchedulerServices scheduler;
  std::shared_ptr<cron::use_scheduled::Flight> interactive;
  std::shared_ptr<cron::print::HeadlessCron> headless;
  std::function<void()> unsubscribeGuard;
};
// settings returns the current merged settings object; cronEnabled (default
// on) and cronDurable (default off) are read from it.
std::shared_ptr<CronWiring> makeCronWiring(event_loop::Loop& owner,
  std::string projectRoot,std::string flightId,std::function<Json()> settings,
  std::function<void(const std::string&)> log);
// CronCreate, CronDelete and CronList; each is enabled only while cron is.
std::vector<gadgets::Gadget> makeCronGadgets(event_loop::Loop& owner,
  const std::shared_ptr<CronWiring>& wiring);
// Interactive: a fired job shows a "Running scheduled task" line and queues
// its prompt behind anything already queued; nothing fires while a turn runs.
void startInteractiveCron(event_loop::Loop& owner,CronWiring& wiring,
  interactive::QueryGuard& guard,std::shared_ptr<messages::Services> messages,
  std::function<void(const Json&)> enqueue,std::function<void(const Json&)> append);
// Print mode: jobs fire only while stream-json input is open and no turn runs.
void startHeadlessCron(event_loop::Loop& owner,CronWiring& wiring,
  std::function<void(const Json&)> enqueue,std::function<void()> run);
void stopCron(CronWiring& wiring);
}
