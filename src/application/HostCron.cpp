#include "application/HostCron.hpp"
#include "cron/CronSchedulerIndigenous.hpp"
#include "cron/use_scheduled/FireTime.hpp"
#include "format/Truncate.hpp"
#include "gadgets/cron/CronGadgets.hpp"
#include "interactive/gadget_views/GadgetViewModel.hpp"
#include "intl/Intl.hpp"
#include "messages/RuntimeMessages.hpp"
#include "runtime/JsonValue.hpp"
#include "text_width/StringWidth.hpp"
#include <cerrno>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace batbox::application {
namespace {
namespace gv=interactive::gadget_views;
std::string text(const Json& value,const char* name){
  const auto& field=value::property(value,name);
  return field.is_string()?field.get<std::string>():std::string();
}
bool expanded(const Json& options){
  return value::property(options,"verbose")==true||
    value::property(options,"transcriptMode")==true;
}
event_loop::Deferred<event_loop::Unit> done(event_loop::Loop& owner){
  event_loop::Deferred<event_loop::Unit> result(owner);
  result.resolve({});return result;
}
// With durable jobs off the scheduler has no file to lock or watch.
struct NoFileWatcher final:cron::SchedulerWatcher {
  void close()override{}
};
std::system_error noDurableFile(){
  return std::system_error(ENOENT,std::generic_category(),
    "durable scheduled jobs are off (cronDurable)");
}
gadgets::cron::CronGadgetUI cronViews(){
  gadgets::cron::CronGadgetUI ui;
  ui.renderCreateResult=[](const Json& output,const Json&,const Json& options)->Json{
    const bool recurring=value::property(output,"recurring")==true;
    const bool durable=value::property(output,"durable")==true;
    return gv::textView("Scheduled "+text(output,"humanSchedule")+
      (recurring?" (recurring)":" (once)")+(durable?" · saved":" · this session")+
      " · "+text(output,"id"),"normal",expanded(options));};
  ui.renderDeleteResult=[](const Json& output,const Json&,const Json& options)->Json{
    return gv::textView("Cancelled "+text(output,"id"),"muted",expanded(options));};
  ui.renderListResult=[](const Json& output,const Json&,const Json& options)->Json{
    const auto& jobs=value::property(output,"jobs");
    if(!jobs.is_array()||jobs.empty())
      return gv::textView("No scheduled jobs","muted",expanded(options));
    std::string lines;
    for(const auto& job:jobs){
      if(!lines.empty())lines+='\n';
      lines+=text(job,"id")+"  "+text(job,"humanSchedule")+"  "+text(job,"prompt");
    }
    return gv::textView(lines,"normal",expanded(options));};
  return ui;
}
}
std::shared_ptr<CronWiring> makeCronWiring(event_loop::Loop& owner,
    std::string projectRoot,std::string flightId,std::function<Json()> settings,
    std::function<void(const std::string&)> log){
  if(!settings||!log)throw std::invalid_argument("Cron settings and log bindings required");
  auto wiring=std::make_shared<CronWiring>();
  wiring->state=std::make_shared<bootstrap::BootstrapState>();
  wiring->state->identity.directories.originalCwd=projectRoot;
  wiring->state->identity.directories.projectRoot=projectRoot;
  wiring->state->identity.directories.cwd=projectRoot;
  wiring->state->identity.flightId=std::move(flightId);
  wiring->cleanup=std::make_shared<cleanup::Roster>(owner);
  wiring->gates=cron::config::indigenousCronConfigurationServices(
    [settings](const std::string& key,const Json& fallback,double){
      const auto current=settings();
      const auto& configured=value::property(current,key.c_str());
      return configured.is_null()||configured.is_discarded()?fallback:configured;
    },true);
  wiring->gates.enabledKey="cronEnabled";
  wiring->gates.durableKey="cronDurable";
  wiring->gates.jitterKey="cronJitter";
  wiring->durable=cron::config::isDurableCronEnabled(wiring->gates);
  wiring->missions=cron::makeIndigenousCronMissionServices(owner,wiring->state,
    [log](std::exception_ptr error){
      try{if(error)std::rethrow_exception(error);}
      catch(const std::exception& failure){log(std::string("[ScheduledTasks] ")+failure.what());}
      catch(...){log("[ScheduledTasks] unknown error");}},
    log);
  if(!wiring->durable){
    wiring->missions.readFile=[&owner](const std::string&){
      event_loop::Deferred<std::string> result(owner);
      result.reject(std::make_exception_ptr(noDurableFile()));return result;};
    wiring->missions.readFileSync=[](const std::string&)->std::string{
      throw noDurableFile();};
    wiring->missions.mkdir=[&owner](const std::string&){
      event_loop::Deferred<event_loop::Unit> result(owner);
      result.reject(std::make_exception_ptr(noDurableFile()));return result;};
    wiring->missions.writeFile=[&owner](const std::string&,const std::string&){
      event_loop::Deferred<event_loop::Unit> result(owner);
      result.reject(std::make_exception_ptr(noDurableFile()));return result;};
  }
  wiring->scheduler=cron::makeIndigenousSchedulerServices(owner,wiring->state,
    *wiring->cleanup,wiring->missions);
  if(!wiring->durable){
    wiring->scheduler.acquireLock=[&owner](const auto&,const auto&){
      event_loop::Deferred<bool> result(owner);result.resolve(true);return result;};
    wiring->scheduler.releaseLock=[&owner](const auto&,const auto&){return done(owner);};
    wiring->scheduler.watchFile=[](const auto&,auto,auto,auto){
      return std::static_pointer_cast<cron::SchedulerWatcher>(
        std::make_shared<NoFileWatcher>());};
  }
  return wiring;
}
std::vector<gadgets::Gadget> makeCronGadgets(event_loop::Loop& owner,
    const std::shared_ptr<CronWiring>& wiring){
  const auto gates=std::make_shared<cron::config::Services>(wiring->gates);
  const bool durable=wiring->durable;
  auto intl=std::make_shared<intl::Stash>(intl::indigenousServices());
  auto width=std::make_shared<text_width::StringWidth>(text_width::indigenousServices(intl));
  auto services=gadgets::cron::makeIndigenousCronGadgetServices(owner,wiring->state,
    wiring->missions,[gates]{return cron::config::isCronEnabled(*gates);},
    [durable]{return durable;},
    []()->std::optional<std::string>{return std::nullopt;},
    format::bindTruncate(format::indigenousTruncateServices(intl,width)),cronViews());
  return {gadgets::cron::makeCronCreateGadget(services),
    gadgets::cron::makeCronDeleteGadget(services),
    gadgets::cron::makeCronListGadget(services)};
}
void startInteractiveCron(event_loop::Loop& owner,CronWiring& wiring,
    interactive::QueryGuard& guard,std::shared_ptr<messages::Services> messages,
    std::function<void(const Json&)> enqueue,std::function<void(const Json&)> append){
  if(wiring.interactive||wiring.headless)return;
  const auto gates=std::make_shared<cron::config::Services>(wiring.gates);
  cron::use_scheduled::Services services;
  services.scheduler=wiring.scheduler;
  services.createScheduler=cron::use_scheduled::indigenousSchedulerFactory(owner,
    wiring.scheduler);
  services.cronEnabled=[gates]{return cron::config::isCronEnabled(*gates);};
  services.getJitterConfig=[gates]{return cron::config::getCronJitterConfig(*gates);};
  services.enqueuePendingNotification=std::move(enqueue);
  // Jobs are never tied to a partner here: CronCreate records no Bat id.
  services.getCurrentMissions=[]{return Json::array();};
  services.findPartnerMissionByBatId=[](const std::string&,const Json&)
      ->std::optional<cron::use_scheduled::PartnerMissionHandle>{return std::nullopt;};
  services.isTerminalMissionStatus=[](const std::string&){return true;};
  services.injectUserMessageToPartner=[](const std::string&,const std::string&){
    throw std::logic_error("Scheduled jobs have no partner to message");};
  services.removeCronMissions=[&owner,missions=wiring.missions](
      const std::vector<std::string>& ids){
    (void)cron::removeCronMissions(owner,ids,std::nullopt,missions);};
  services.logForDebugging=wiring.missions.logDebug;
  services.createScheduledMissionFireMessage=[messages](const std::string& content){
    auto message=messages::createSystemMessage(content,"info",*messages);
    message["subtype"]="scheduled_task_fire";
    return message;};
  services.appendMessageAtomically=std::move(append);
  services.formatCronFireTime=cron::use_scheduled::indigenousCronFireTimeFormatter();
  wiring.interactive=std::make_shared<cron::use_scheduled::Flight>(owner,std::move(services));
  wiring.interactive->render(guard.isActive(),false);
  wiring.interactive->commit();
  std::weak_ptr<cron::use_scheduled::Flight> weak=wiring.interactive;
  wiring.unsubscribeGuard=guard.subscribe(
    std::make_shared<bootstrap::Signal<>::Listener>([weak,&guard]{
      if(const auto flight=weak.lock())flight->render(guard.isActive(),false);}));
}
void startHeadlessCron(event_loop::Loop& owner,CronWiring& wiring,
    std::function<void(const Json&)> enqueue,std::function<void()> run){
  if(wiring.interactive||wiring.headless)return;
  const auto gates=std::make_shared<cron::config::Services>(wiring.gates);
  cron::print::Services services;
  services.batTriggersBuilt=[]{return true;};
  services.schedulerModuleAvailable=true;
  services.cronEnabled=[gates]{return cron::config::isCronEnabled(*gates);};
  services.getJitterConfig=[gates]{return cron::config::getCronJitterConfig(*gates);};
  services.createScheduler=cron::use_scheduled::indigenousSchedulerFactory(owner,
    wiring.scheduler);
  services.enqueue=std::move(enqueue);
  services.randomUUID=wiring.missions.randomUUID;
  services.run=std::move(run);
  wiring.headless=std::make_shared<cron::print::HeadlessCron>(std::move(services));
  wiring.headless->start();
}
void stopCron(CronWiring& wiring){
  if(wiring.unsubscribeGuard){wiring.unsubscribeGuard();wiring.unsubscribeGuard={};}
  if(wiring.interactive){
    try{wiring.interactive->unmount();}catch(...){}
    wiring.interactive.reset();
  }
  if(wiring.headless){wiring.headless->closeInput();wiring.headless.reset();}
}
}
