#include "gadgets/cron/CronGadgetInternal.hpp"
#include "gadgets/cron/CronPoetry.hpp"
#include "cron/CronExpression.hpp"
#include "cron/config/CronConfiguration.hpp"
#include "event_loop/AsyncContext.hpp"
#include <future>
#include <memory>

namespace batbox::gadgets::cron {
Gadget makeCronCreateGadget(CronGadgetServices services){
  require(bool(services.cronEnabled),"cronEnabled");
  require(bool(services.durableEnabled),"durableEnabled");
  require(bool(services.nowMs),"nowMs");
  require(bool(services.partnerBatId),"teammateAgentId");
  require(bool(services.cronFilePath),"cronFilePath");
  require(bool(services.cronToHuman),"cronToHuman");
  require(bool(services.listMissions),"listTasks");
  require(bool(services.addMission),"addTask");
  require(bool(services.setScheduledMissionsEnabled),
    "setScheduledTasksEnabled");
  require(bool(services.truncate),"truncate");
  require(bool(services.ui.renderCreateResult),"renderCreateResult");
  auto shared=std::make_shared<CronGadgetServices>(std::move(services));
  Gadget gadget;
  gadget.name=createName;
  gadget.searchHint="schedule a recurring or one-shot prompt";
  gadget.maxResultSizeChars=100000;gadget.shouldDefer=true;
  gadget.inputSchema=cronInputSchema(gadget.name);
  gadget.inputJSONSchema=gadget.inputSchema;
  gadget.outputSchema=cronOutputSchema(gadget.name);
  gadget.parseInput=parseCronCreateInput;
  gadget.isEnabled=shared->cronEnabled;
  gadget.toAutoClassifierInput=[](const Json& input)->Json{
    return field(input,"cron")+": "+field(input,"prompt");
  };
  gadget.description=[shared](const Json&,const Json&){
    return ready(buildCronCreateDescription(shared->durableEnabled()));
  };
  gadget.poetry=[shared](const PoetryContext&){
    return ready(buildCronCreatePoetry(shared->durableEnabled()));
  };
  gadget.getPath=[shared](const Json&){return shared->cronFilePath();};
  gadget.validateInput=[shared](const Json& input,const Json&){
    return std::async(std::launch::deferred,
      runtime::bindAsyncContext([shared,input]{
      const auto expression=field(input,"cron");
      if(!batbox::cron::parseCronExpression(expression))
        return invalidValidation(
          "Invalid cron expression '"+expression+
          "'. Expected 5 fields: M H DoM Mon DoW.",1);
      if(!batbox::cron::nextCronRunMs(expression,shared->nowMs()))
        return invalidValidation(
          "Cron expression '"+expression+
          "' does not match any calendar date in the next year.",2);
      const auto missions=shared->listMissions().get();
      if(missions.size()>=50)return invalidValidation(
        "Too many scheduled jobs (max 50). Cancel one first.",3);
      if(input.value("durable",false)&&shared->partnerBatId())
        return invalidValidation(
          "durable crons are not supported for Partners (Partners do not persist across Flights)",4);
      return ValidationResult{};
    }));
  };
  gadget.call=[shared](const Json& input,const GadgetCallContext&,
    const std::function<std::future<Json>(const Json&)>&,const Json&,
    const std::function<void(const GadgetProgress&)>&){
    return std::async(std::launch::deferred,
      runtime::bindAsyncContext([shared,input]{
      const auto expression=field(input,"cron");
      const auto poetry=field(input,"prompt");
      const bool recurring=input.value("recurring",true);
      const bool durable=input.value("durable",false);
      const bool effectiveDurable=durable&&shared->durableEnabled();
      const auto id=shared->addMission(expression,poetry,recurring,
        effectiveDurable,shared->partnerBatId()).get();
      shared->setScheduledMissionsEnabled(true);
      GadgetResult result;result.data=Json::object();
      result.data["id"]=id;
      result.data["humanSchedule"]=shared->cronToHuman(expression);
      result.data["recurring"]=recurring;
      result.data["durable"]=effectiveDurable;
      return result;
    }));
  };
  gadget.mapGadgetResultToGadgetResultBlockParam=[](const Json& output,
    const std::string& gadgetUseID){
    const bool durable=output.value("durable",false);
    const auto where=durable?
      "Persisted to .batbox/scheduled_tasks.json":
      "Session-only (not written to disk, dies when Batbox exits)";
    const auto id=field(output,"id"),human=field(output,"humanSchedule");
    const bool recurring=output.value("recurring",false);
    const auto age=std::to_string(static_cast<int>(
      batbox::cron::config::maxAgeDays));
    const auto content=recurring?
      "Scheduled recurring job "+id+" ("+human+"). "+where+
        ". Auto-expires after "+age+
        " days. Use CronDelete to cancel sooner.":
      "Scheduled one-shot task "+id+" ("+human+"). "+where+
        ". It will fire once then auto-delete.";
    return Json{{"tool_use_id",gadgetUseID},{"type","tool_result"},
      {"content",content}};
  };
  gadget.renderGadgetUseMessage=[shared](const Json& input,const Json&)->Json{
    const auto expression=field(input,"cron"),poetry=field(input,"prompt");
    return expression+(poetry.empty()?"":": "+shared->truncate(poetry,60,true));
  };
  gadget.renderGadgetResultMessage=shared->ui.renderCreateResult;
  return buildGadget(std::move(gadget));
}
}
