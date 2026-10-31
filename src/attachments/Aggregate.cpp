#include "attachments/Aggregate.hpp"
#include "query/OwnerPromiseAll.hpp"
#include <memory>
#include <stdexcept>
#include <utility>

namespace batbox::attachments {
namespace {
using event_loop::Deferred;
const Json& field(const Json& value,const char* key){return value::property(value,key);}
void appendFlat(Json& output,const Json& value){
  if(value.is_array())for(const auto& item:value){
    if(!item.is_null()&&!item.is_discarded())output.push_back(item);
  }
  else if(!value.is_null()&&!value.is_discarded())output.push_back(value);
}
struct Run:std::enable_shared_from_this<Run> {
  Input input;
  execution::Context& original;
  execution::Context timed;
  Services services;
  std::stop_source stop;
  event_loop::Loop::TimerHandle timeout;
  Deferred<Json> output;
  std::vector<Json> userResults;
  bool isMainThread=false;
  Run(Input args,execution::Context& context,Services imports):
    input(std::move(args)),original(context),timed(context),
    services(std::move(imports)),output(services.loop){}
  bool feature(const char* key){
    if(!services.feature)
      throw std::invalid_argument("Attachment feature import required");
    return services.feature(key);
  }
  void fail(const std::exception_ptr& error){output.reject(error);}
  Deferred<Json> maybe(const char* label,
      const std::function<Deferred<Json>()>& start){
    Deferred<Json> wrapped(services.loop);
    auto failed=[wrapped,label=std::string(label),services=services](
      const std::exception_ptr& error){
      try{
        if(!services.logError||!services.logDiagnostic)
          throw std::invalid_argument("Attachment error log imports required");
        services.logError(error);
        services.logDiagnostic("Attachment error in "+label,error);
        wrapped.resolve(Json::array());
      }catch(...){wrapped.reject(std::current_exception());}
    };
    try{
      auto pending=start();
      if(!services.loop.dispatcher().sameLoop(pending.dispatcher()))
        throw std::invalid_argument("Attachment collector crossed event loops");
      pending.then([wrapped,failed](const Json* value,
          const std::exception_ptr& error){
        if(error){failed(error);return;}
        wrapped.resolve(value?*value:Json::array());
      });
    }catch(...){failed(std::current_exception());}
    return wrapped;
  }
  Deferred<Json> asAsync(const std::function<Deferred<Json>()>& start){
    try{return start();}
    catch(...){
      Deferred<Json> rejected(services.loop);
      rejected.reject(std::current_exception());
      return rejected;
    }
  }
  void add(std::vector<Deferred<Json>>& out,const char* label,
      const Collector& collector,bool useTimed,bool asyncSource=false){
    if(!collector)throw std::invalid_argument(
      "Unbound attachment collector: "+std::string(label));
    out.push_back(maybe(label,[this,&collector,useTimed,asyncSource]{
      if(asyncSource)return asAsync([this,&collector,useTimed]{
        return collector(input,useTimed?timed:original);
      });
      return collector(input,useTimed?timed:original);
    }));
  }
  void queued(std::vector<Deferred<Json>>& out){
    out.push_back(maybe("queued_commands",[this]{
      return asAsync([this]{
        return interactive::background::getQueuedCommandAttachments(
          services.loop,input.queuedCommands,services.notifications);
      });
    }));
  }
  void nested(std::vector<Deferred<Json>>& out){
    if(!services.triggers)throw std::invalid_argument(
      "Nested-memory attachment bindings required");
    out.push_back(maybe("nested_memory",[this]{
      return asAsync([this]{
        return attachment_triggers::getNestedMemoryAttachments(
          timed,*services.triggers);
      });
    }));
  }
  void dynamic(std::vector<Deferred<Json>>& out){
    if(!services.triggers)throw std::invalid_argument(
      "Dynamic-skill attachment bindings required");
    out.push_back(maybe("dynamic_skill",[this]{
      return asAsync([this]{
        return attachment_triggers::getDynamicSkillAttachments(
          timed,*services.triggers);
      });
    }));
  }
  void start(){
    try{
      if(!services.disableAttachments||!services.simpleMode)
        throw std::invalid_argument("Attachment environment imports required");
      if(services.disableAttachments()||services.simpleMode()){
        interactive::background::getQueuedCommandAttachments(
          services.loop,input.queuedCommands,services.notifications).then(
          [self=shared_from_this()](const Json* value,
            const std::exception_ptr& error){
            if(error)self->fail(error);
            else self->output.resolve(value?*value:Json::array());
          });
        return;
      }
      timed.stop=stop.get_token();
      timeout=services.loop.setTimeout(1000,[source=stop]() mutable {source.request_stop();});
      isMainThread=!value::truthy(field(original.data,"agentId"));
      std::vector<Deferred<Json>> user;
      if(input.text&&!input.text->empty()){
        const auto& c=services.collectors;
        add(user,"at_mentioned_files",c.atMentionedFiles,true,true);
        add(user,"mcp_resources",c.mcpResources,true,true);
        add(user,"agent_mentions",c.batMentions,true);
        if(feature("EXPERIMENTAL_SKILL_SEARCH")){
          if(!services.skillSearchModulesPresent)
            throw std::invalid_argument("Skill search module import required");
          if(services.skillSearchModulesPresent()&&!input.skipSkillDiscovery)
            add(user,"skill_discovery",c.skillDiscovery,true);
        }
      }
      query::ownerPromiseAll(services.loop,user).then(
        [self=shared_from_this()](const std::vector<Json>* values,
          const std::exception_ptr& error){
          if(error){self->fail(error);return;}
          try{self->userResults=*values;self->afterUser();}
          catch(...){self->fail(std::current_exception());}
        });
    }catch(...){fail(std::current_exception());}
  }
  void afterUser(){
    const auto& c=services.collectors;
    std::vector<Deferred<Json>> thread,main;
    queued(thread);
    add(thread,"date_change",c.dateChange,false);
    add(thread,"ultrathink_effort",c.ultrathinkEffort,false);
    add(thread,"deferred_tools_delta",c.deferredGadgetsDelta,false);
    add(thread,"agent_listing_delta",c.batListingDelta,false);
    add(thread,"mcp_instructions_delta",c.mcpInstructionsDelta,false);
    if(feature("BUDDY"))add(thread,"companion_intro",c.companionIntro,false);
    add(thread,"changed_files",c.changedFiles,true,true);
    nested(thread);
    dynamic(thread);
    add(thread,"skill_listing",c.skillListing,true,true);
    add(thread,"plan_mode",c.planMode,false,true);
    add(thread,"plan_mode_exit",c.planModeExit,false,true);
    if(feature("TRANSCRIPT_CLASSIFIER")){
      add(thread,"auto_mode",c.autoMode,false,true);
      add(thread,"auto_mode_exit",c.autoModeExit,false,true);
    }
    thread.push_back(maybe("todo_reminders",[this,&c]{
      if(!services.isTodoV2Enabled)
        throw std::invalid_argument("Todo version import required");
      const auto& selected=services.isTodoV2Enabled()?c.missionReminders:
        c.todoReminders;
      if(!selected)
        throw std::invalid_argument("Unbound todo reminder collector");
      return asAsync([this,&selected]{return selected(input,original);});
    }));
    if(!services.isBatSwarmsEnabled)
      throw std::invalid_argument("Agent swarms import required");
    if(services.isBatSwarmsEnabled()){
      if(input.querySource!="session_memory")
        add(thread,"teammate_mailbox",c.partnerMailbox,false,true);
      add(thread,"team_context",c.teamContext,false,true);
    }
    add(thread,"agent_pending_messages",c.batPendingMessages,false,true);
    add(thread,"critical_system_reminder",c.criticalSystemReminder,false);
    if(feature("COMPACTION_REMINDERS"))
      add(thread,"compaction_reminder",c.compactionReminder,false);
    if(feature("HISTORY_SNIP"))
      add(thread,"context_efficiency",c.contextEfficiency,false);
    if(isMainThread){
      add(main,"ide_selection",c.selectedLinesIde,false,true);
      add(main,"ide_opened_file",c.openedFileIde,false,true);
      add(main,"output_style",c.outputStyle,false,true);
      add(main,"diagnostics",c.diagnostics,false,true);
      add(main,"lsp_diagnostics",c.lspDiagnostics,false,true);
      add(main,"unified_tasks",c.unifiedMissions,false,true);
      add(main,"async_hook_responses",c.asyncHookResponses,false,true);
      add(main,"token_usage",c.tokenUsage,false,true);
      add(main,"budget_usd",c.budgetUsd,false,true);
      add(main,"output_token_usage",c.outputTokenUsage,false,true);
      add(main,"verify_plan_reminder",c.verifyPlanReminder,false,true);
    }
    auto allThread=query::ownerPromiseAll(services.loop,thread);
    auto allMain=query::ownerPromiseAll(services.loop,main);
    query::ownerPromiseAll(services.loop,
      std::vector<Deferred<std::vector<Json>>>{allThread,allMain}).then(
      [self=shared_from_this()](
          const std::vector<std::vector<Json>>* values,
          const std::exception_ptr& error){
        if(error){self->fail(error);return;}
        try{
          self->timeout.cancel();
          Json attachments=Json::array();
          for(const auto& value:self->userResults)appendFlat(attachments,value);
          for(const auto& value:values->at(0))appendFlat(attachments,value);
          for(const auto& value:values->at(1))appendFlat(attachments,value);
          self->output.resolve(std::move(attachments));
        }catch(...){self->fail(std::current_exception());}
      });
  }
};
}
Deferred<Json> getAttachments(Input input,execution::Context& context,
  const Services& services){
  if(!services.loop.isOwnerThread())
    throw std::logic_error("Attachment aggregation requires owner loop");
  auto run=std::make_shared<Run>(std::move(input),context,services);
  run->start();return run->output;
}
} // namespace batbox::attachments
