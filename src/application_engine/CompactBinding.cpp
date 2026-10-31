#include "application_engine/CompactBinding.hpp"
#include "application_engine/AutoPolicy.hpp"
#include "application_engine/CompactFork.hpp"
#include "application/Async.hpp"
#include "compaction/Compaction.hpp"
#include "attachments/files/FileAttachments.hpp"
#include "attachments/context/ContextCollectors.hpp"
#include "interactive/background/NotificationAttachments.hpp"
#include "messages/RuntimeMessages.hpp"
#include "retry/Retry.hpp"
#include "runtime/Platform.hpp"
#include "runtime/IndigenousPath.hpp"
#include "runtime/OrderedStringSet.hpp"
#include <fstream>

namespace batbox::application_engine {
namespace {
Json stashJson(const std::shared_ptr<bats::StashSafeParams>& value){
  if(!value)throw std::invalid_argument("Native compaction snapshot required");
  return {{"systemPrompt",value->systemPoetry},{"userContext",value->userContext},
    {"systemContext",value->systemContext},{"toolUseContext",value->gadgetUseContext.query.data},
    {"forkContextMessages",value->forkContextMessages}};
}
Json discoveredGadgets(const Json& messages){
  runtime::OrderedStringSet names;
  for(const auto& message:messages){
    if(value::property(message,"type")=="system"&&
        value::property(message,"subtype")=="compact_boundary"){
      const auto& carried=value::property(value::property(message,"compactMetadata"),"preCompactDiscoveredTools");
      if(carried.is_array())for(const auto& name:carried)names.insert(name.get<std::string>());
      continue;
    }
    if(value::property(message,"type")!="user")continue;
    const auto& content=value::property(value::property(message,"message"),"content");
    if(!content.is_array())continue;
    for(const auto& block:content){
      const auto& items=value::property(block,"content");
      if(value::property(block,"type")!="tool_result"||!items.is_array())continue;
      for(const auto& item:items)
        if(value::property(item,"type")=="tool_reference"&&value::property(item,"tool_name").is_string())
          names.insert(item.at("tool_name").get<std::string>());
    }
  }
  Json result=Json::array();for(const auto& name:names)result.push_back(name);
  return result;
}
}
std::shared_ptr<Compactor> makeCompactor(CompactBindingInputs inputs){
  if(!inputs.model||!inputs.messages||!inputs.hooks||!inputs.poetry||
      !inputs.flight||!inputs.store||!inputs.permission||!inputs.readServices||
      !inputs.normalizeAttachment)
    throw std::invalid_argument("Native compaction provider graph required");
  if(!inputs.invokedSkills)inputs.invokedSkills=std::make_shared<bootstrap::InvokedSkillsState>();
  auto host=std::make_shared<CompactBindingInputs>(std::move(inputs));
  const auto owner=host->owner.dispatcher();
  CompactorInputs compact(host->owner);
  compact.config=host->config;compact.model=host->model;compact.messages=host->messages;
  compact.normalize=[model=host->model](const Json& messages,const gadgets::Gadgets& gadgets){
    return model->normalizeMessagesForAPI(messages,gadgets);
  };
  compact.normalizeAttachment=host->normalizeAttachment;
  compact.getAppState=[store=host->store](const execution::Context& context){
    return execution::effectiveAppState(context,store->getState()->snapshot());
  };
  compact.autoCompactEnabled=[owner,store=host->store]{
    return application::onOwnerAndWait<bool>(owner,[store]{
      return autoCompactSetting(store->getState()->snapshot());});
  };
  compact.compactionSettings=[owner,store=host->store]{
    return application::onOwnerAndWait<Json>(owner,[store]{
      return value::property(store->getState()->snapshot(),"settings");});
  };
  compact.progress=host->progress;compact.notification=host->notification;
  // Remote analytics collection is disabled; this callback does not
  // invalidate poetry stashes.
  compact.markPostCompaction=[]{ };
  compact.cleanupAfterCompact=[host](const std::optional<std::string>& source){
    compaction::CleanupServices cleanup;
    cleanup.feature=host->messages->featureGate;
    // The enabled indigenous time-based microcompactor has no retained state.
    cleanup.resetMicrocompactState=[]{ };
    cleanup.resetContextCollapse=[]{
      throw std::logic_error("Context collapse cleanup needs a native provider");
    };
    cleanup.clearUserContextStash=[poetry=host->poetry]{
      poetry->contextStash().clearUserContextStash();
    };
    cleanup.resetGetMemoryFilesStash=[poetry=host->poetry](const std::string& reason){
      if(reason!="compact")throw std::invalid_argument("Unexpected memory reset reason");
      poetry->resetProjectInstructionsAfterCompact();
    };
    cleanup.clearSystemPoetrySections=[poetry=host->poetry]{
      poetry->clearPoetrySections();
    };
    // Classifier approval and speculative-check owners are disabled in the
    // indigenous local profile; neither holds stash state to invalidate.
    cleanup.clearClassifierApprovals=[]{ };
    cleanup.clearSpeculativeChecks=[]{ };
    cleanup.clearFlightMessagesStash=[flight=host->flight]{
      flight->clearMessageStash();
    };
    compaction::runPostCompactCleanup(source,cleanup);
  };
  compact.clearInvokedSkills=[state=host->invokedSkills](
      const std::vector<std::string>& preserved){
    bootstrap::clearInvokedSkills(*state,preserved);
  };
  auto& workflow=compact.workflow;
  workflow.feature=host->messages->featureGate;
  workflow.poetry.feature=workflow.feature;
  workflow.poetry.isProactiveActive=[]{return false;};
  workflow.createAttachmentMessage=[messages=host->messages](const Json& value){
    return *interactive::background::createAttachmentMessage(value,
      messages->randomUUID,messages->nowISO);
  };
  workflow.executePreCompactHooks=[host,owner](const Json& detail,std::stop_token signal){
    const auto& value=value::property(detail,"customInstructions");
    const auto instructions=value.is_string()?std::optional(value.get<std::string>()):std::nullopt;
    const auto trigger=detail.at("trigger").get<std::string>();
    const auto result=application::awaitOnOwner<application_hooks::hooks::CompactHookOutput>(owner,
      [host,trigger,instructions,signal]{return host->hooks->preCompact(trigger,instructions,signal);});
    Json out=Json::object();
    if(result.newCustomInstructionsOwnProperty)out["newCustomInstructions"]=result.newCustomInstructions?
      Json(*result.newCustomInstructions):Json();
    if(result.userDisplayMessageOwnProperty)out["userDisplayMessage"]=result.userDisplayMessage?
      Json(*result.userDisplayMessage):Json();
    return out;
  };
  workflow.executePostCompactHooks=[host,owner](const Json& detail,std::stop_token signal){
    const auto trigger=detail.at("trigger").get<std::string>();
    const auto summary=detail.at("compactSummary").get<std::string>();
    const auto result=application::awaitOnOwner<application_hooks::hooks::CompactHookOutput>(owner,
      [host,trigger,summary,signal]{return host->hooks->postCompact(trigger,summary,signal);});
    Json out=Json::object();
    if(result.userDisplayMessageOwnProperty)out["userDisplayMessage"]=result.userDisplayMessage?
      Json(*result.userDisplayMessage):Json();
    return out;
  };
  workflow.processFlightStartHooks=[host,owner](const std::string& source,const Json& args){
    auto detail=args;detail["source"]=source;
    const auto result=application::awaitOnOwner<
      application_hooks::FlightStartOutput>(owner,[host,owner,source,detail]{
      event_loop::Deferred<application_hooks::FlightStartOutput> output(host->owner);
      host->hooks->run("SessionStart",detail,source).then(
        [output,hooks=host->hooks,update=host->updateWatchPaths](
            const std::vector<Json>* events,
            const std::exception_ptr& error){
          if(error){output.reject(error);return;}
          try{
            auto collected=hooks->collectFlightStart(events?*events:
              std::vector<Json>{});
            if(update&&!collected.watchPaths.empty())
              update(collected.watchPaths);
            output.resolve(std::move(collected));
          }
          catch(...){output.reject(std::current_exception());}
        });
      return output;
    });
    return result.messages;
  };
  workflow.getTranscriptPath=[flight=host->flight,owner]{
    return application::onOwnerAndWait<std::string>(owner,[flight]{
      return flight->paths()->getTranscriptPath();
    });
  };
  workflow.reAppendFlightMetadata=[host,owner]{
    application::onOwnerAndWait<void>(owner,[host]{host->flight->reappendMetadata();});
  };
  workflow.extractDiscoveredGadgetNames=discoveredGadgets;
  // These modes keep their instructions in system poetry until
  // indigenous deferred-schema/bat-list feature providers are enabled.
  workflow.getDeferredGadgetsDeltaAttachment=[](const Json&,const Json&,const Json&,const Json&){return Json::array();};
  workflow.getBatListingDeltaAttachment=[](compaction::Context&,const Json&){return Json::array();};
  workflow.getMcpInstructionsDeltaAttachment=[](const Json&,const Json&,const Json&,const Json&){return Json::array();};
  CompactForkInputs fork(host->owner);
  fork.messages=host->messages;
  fork.flight=host->flight;
  fork.currentEngine=host->currentEngine;
  fork.notifyCommandLifecycle=host->notifyCommandLifecycle;
  bindCompactSummaryFork(compact.summary,std::move(fork));
  compact.summary.streamingRetryEnabled=[]{return true;};
  compact.summary.isGadgetSearchEnabled=[](const Json&,const Json&,const std::function<Json()>&,
      const Json&,const std::string&){return false;};
  compact.summary.getRetryDelay=[](double attempt){
    retry::Dependencies dependencies;dependencies.random=platform::randomUnitInterval;
    return retry::getRetryDelay(attempt,{},32000,dependencies);
  };
  compact.summary.sleep=retry::sleepAbortResponsive;
  compact.makeRestore=[host,owner,attachment=workflow.createAttachmentMessage](
      const execution::Context& query){
    compaction::RestoreServices restore;
    restore.createAttachmentMessage=attachment;
    restore.expandPath=[cwd=host->config.cwd](const std::string& path){
      const auto value=std::filesystem::path(path);
      return (value.is_absolute()?value:cwd/value).lexically_normal().string();
    };
    const auto memories=application::awaitOnOwner<context::MemoryFiles>(owner,[host]{
      return host->poetry->memoryFiles();
    });
    for(const auto& memory:memories)restore.memoryTypes.push_back(memory.path);
    restore.getMemoryPath=[](const std::string& path){return path;};
    const auto planPath=[flight=host->flight,owner](const Json& bat){
      return application::onOwnerAndWait<std::string>(owner,[flight,bat]{
        return runtime::pathToUtf8(flight->planFilePath(bat.is_string()
          ?std::optional(bat.get<std::string>()):std::nullopt));
      });
    };
    restore.getPlanFilePath=planPath;
    restore.getPlan=[planPath](const Json& bat)->Json{
      std::ifstream input(planPath(bat),std::ios::binary);
      if(!input)return Json();
      return std::string(std::istreambuf_iterator<char>(input),{});
    };
    restore.getMissionOutputPath=[flight=host->flight,owner](const std::string& id){
      return application::onOwnerAndWait<std::string>(owner,[flight,id]{
        return flight->paths()->getBatTranscriptPath(id);
      });
    };
    restore.getInvokedSkillsForBat=[host,owner](const Json& bat){
      return application::onOwnerAndWait<Json>(owner,[host,bat]{
        const auto id=bat.is_string()?std::optional(bat.get<std::string>()):std::nullopt;
        Json records=Json::array();
        for(const auto& [key,skill]:bootstrap::getInvokedSkillsForBat(*host->invokedSkills,id)){
          (void)key;
          records.push_back({{"skillName",skill->skillName},{"skillPath",skill->skillPath},
            {"content",skill->content},{"invokedAt",skill->invokedAt}});
        }
        return records;
      });
    };
    restore.generateFileAttachment=[host,owner,query](const std::string& filename,
        compaction::Context& compactContext,double budget,const std::string&){
      auto result=std::make_shared<std::promise<Json>>();auto future=result->get_future();
      auto turn=std::make_shared<execution::Context>(query);
      turn->stop=compactContext.signal;
      owner.postMission([host,result,turn,filename,budget]{
        try{
          auto read=std::make_shared<gadgets::files::FileReadServices>(host->readServices());
          read->defaultLimits.maxTokens=static_cast<std::size_t>(budget);
          auto services=attachments::files::makeIndigenousFileServices(host->owner,*read,
            [store=host->store]{return store->getState();},
            [permission=host->permission](const std::string& path,const app_state::AppStateRef& app){
              return permission->deniedByRule("Read",path,app->snapshot());
            });
          attachments::files::generateFileAttachment(filename,*turn,
            attachments::files::Mode::Compact,{},services).then(
              [result,turn,read](const std::optional<Json>* value,const std::exception_ptr& error){
                if(error)result->set_exception(error);
                else result->set_value(value&&*value?**value:Json());
              });
        }catch(...){result->set_exception(std::current_exception());}
      });
      return future;
    };
    return restore;
  };
  return Compactor::create(std::move(compact));
}
void bindCompactor(Inputs& engine,std::shared_ptr<Compactor> compactor){
  if(!compactor)throw std::invalid_argument("Compaction owner required");
  engine.autocompact=[compactor](const Json& messages,const execution::Context& context,
      const Json& stash,const std::string& source,const Json& tracking,double freed){
    return compactor->automatic(messages,context,stash,source,tracking,freed);
  };
  engine.autocompactIndigenous=[compactor](const Json& messages,const execution::Context& context,
      const std::shared_ptr<bats::StashSafeParams>& stash,const std::string& source,
      const Json& tracking,double freed){
    return compactor->automatic(messages,context,stashJson(stash),source,tracking,freed,stash);
  };
}
} // namespace batbox::application_engine
