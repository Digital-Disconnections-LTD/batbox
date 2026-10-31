#include "gadgets/cron/CronGadgetInternal.hpp"
#include "gadgets/cron/CronPoetry.hpp"
#include "event_loop/AsyncContext.hpp"
#include <algorithm>
#include <future>
#include <memory>

namespace batbox::gadgets::cron {
Gadget makeCronDeleteGadget(CronGadgetServices services){
  require(bool(services.cronEnabled),"cronEnabled");
  require(bool(services.durableEnabled),"durableEnabled");
  require(bool(services.cronFilePath),"cronFilePath");
  require(bool(services.partnerBatId),"teammateAgentId");
  require(bool(services.listMissions),"listTasks");
  require(bool(services.removeMissions),"removeTasks");
  require(bool(services.ui.renderDeleteResult),"renderDeleteResult");
  auto shared=std::make_shared<CronGadgetServices>(std::move(services));
  Gadget gadget;
  gadget.name=deleteName;
  gadget.searchHint="cancel a scheduled cron job";
  gadget.maxResultSizeChars=100000;gadget.shouldDefer=true;
  gadget.inputSchema=cronInputSchema(gadget.name);
  gadget.inputJSONSchema=gadget.inputSchema;
  gadget.outputSchema=cronOutputSchema(gadget.name);
  gadget.parseInput=parseCronDeleteInput;
  gadget.isEnabled=shared->cronEnabled;
  gadget.toAutoClassifierInput=[](const Json& input)->Json{
    return field(input,"id");
  };
  gadget.description=[](const Json&,const Json&){
    return ready(std::string(deleteDescription));
  };
  gadget.poetry=[shared](const PoetryContext&){
    return ready(buildCronDeletePoetry(shared->durableEnabled()));
  };
  gadget.getPath=[shared](const Json&){return shared->cronFilePath();};
  gadget.validateInput=[shared](const Json& input,const Json&){
    return std::async(std::launch::deferred,
      runtime::bindAsyncContext([shared,input]{
      const auto missions=shared->listMissions().get();
      const auto id=field(input,"id");
      const auto found=std::find_if(missions.begin(),missions.end(),
        [&id](const auto& mission){return mission.id==id;});
      if(found==missions.end())return invalidValidation(
        "No scheduled job with id '"+id+"'",1);
      const auto batId=shared->partnerBatId();
      if(batId&&found->batId!=batId)
        return invalidValidation(
          "Cannot delete cron job '"+id+
          "': owned by another agent",2);
      return ValidationResult{};
    }));
  };
  gadget.call=[shared](const Json& input,const GadgetCallContext&,
    const std::function<std::future<Json>(const Json&)>&,const Json&,
    const std::function<void(const GadgetProgress&)>&){
    return std::async(std::launch::deferred,
      runtime::bindAsyncContext([shared,input]{
      const auto id=field(input,"id");
      shared->removeMissions({id}).get();
      GadgetResult result;result.data=Json{{"id",id}};
      return result;
    }));
  };
  gadget.mapGadgetResultToGadgetResultBlockParam=[](const Json& output,
    const std::string& gadgetUseID){
    return Json{{"tool_use_id",gadgetUseID},{"type","tool_result"},
      {"content","Cancelled job "+field(output,"id")+"."}};
  };
  gadget.renderGadgetUseMessage=[](const Json& input,const Json&)->Json{
    return field(input,"id");
  };
  gadget.renderGadgetResultMessage=shared->ui.renderDeleteResult;
  return buildGadget(std::move(gadget));
}
}
