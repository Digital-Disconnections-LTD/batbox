#include "gadgets/cron/CronGadgetInternal.hpp"
#include "gadgets/cron/CronPoetry.hpp"
#include "event_loop/AsyncContext.hpp"
#include <future>
#include <memory>

namespace batbox::gadgets::cron {
Gadget makeCronListGadget(CronGadgetServices services){
  require(bool(services.cronEnabled),"cronEnabled");
  require(bool(services.durableEnabled),"durableEnabled");
  require(bool(services.listMissions),"listTasks");
  require(bool(services.partnerBatId),"teammateAgentId");
  require(bool(services.cronToHuman),"cronToHuman");
  require(bool(services.truncate),"truncate");
  require(bool(services.ui.renderListResult),"renderListResult");
  auto shared=std::make_shared<CronGadgetServices>(std::move(services));
  Gadget gadget;
  gadget.name=listName;
  gadget.searchHint="list active cron jobs";
  gadget.maxResultSizeChars=100000;gadget.shouldDefer=true;
  gadget.inputSchema=cronInputSchema(gadget.name);
  gadget.inputJSONSchema=gadget.inputSchema;
  gadget.outputSchema=cronOutputSchema(gadget.name);
  gadget.parseInput=parseCronListInput;
  gadget.isEnabled=shared->cronEnabled;
  gadget.isConcurrencySafe=[](const Json&){return true;};
  gadget.isReadOnly=[](const Json&){return true;};
  gadget.description=[](const Json&,const Json&){
    return ready(std::string(listDescription));
  };
  gadget.poetry=[shared](const PoetryContext&){
    return ready(buildCronListPoetry(shared->durableEnabled()));
  };
  gadget.call=[shared](const Json&,const GadgetCallContext&,
    const std::function<std::future<Json>(const Json&)>&,const Json&,
    const std::function<void(const GadgetProgress&)>&){
    return std::async(std::launch::deferred,
      runtime::bindAsyncContext([shared]{
      const auto allMissions=shared->listMissions().get();
      const auto partner=shared->partnerBatId();
      Json jobs=Json::array();
      for(const auto& mission:allMissions){
        if(partner&&mission.batId!=partner)continue;
        Json job=Json::object();
        job["id"]=mission.id;job["cron"]=mission.cron;
        job["humanSchedule"]=shared->cronToHuman(mission.cron);
        job["prompt"]=mission.poetry;
        if(mission.recurring.value_or(false))job["recurring"]=true;
        if(mission.durable==false)job["durable"]=false;
        jobs.push_back(std::move(job));
      }
      GadgetResult result;result.data=Json{{"jobs",jobs}};
      return result;
    }));
  };
  gadget.mapGadgetResultToGadgetResultBlockParam=[shared](const Json& output,
    const std::string& gadgetUseID){
    const auto& jobs=output.at("jobs");
    std::string content;
    if(jobs.empty())content="No scheduled jobs.";
    else for(const auto& job:jobs){
      if(!content.empty())content+='\n';
      content+=field(job,"id")+" — "+field(job,"humanSchedule")+
        (job.value("recurring",false)?" (recurring)":" (one-shot)")+
        (job.value("durable",true)?"":" [session-only]")+": "+
        shared->truncate(field(job,"prompt"),80,true);
    }
    return Json{{"tool_use_id",gadgetUseID},{"type","tool_result"},
      {"content",content}};
  };
  gadget.renderGadgetUseMessage=[](const Json&,const Json&)->Json{return "";};
  gadget.renderGadgetResultMessage=shared->ui.renderListResult;
  return buildGadget(std::move(gadget));
}
}
