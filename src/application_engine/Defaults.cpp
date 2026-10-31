#include "application_engine/Defaults.hpp"
#include "application_engine/AutoPolicy.hpp"
#include "application/Async.hpp"
#include "api_errors/ApiErrors.hpp"
#include "instructions/hooks/runtime/HookBlockingMessages.hpp"
#include "interactive/background/NotificationAttachments.hpp"
#include "messages/RuntimeMessages.hpp"
#include "query/MemoryAttachmentFilter.hpp"
#include "runtime/Platform.hpp"
#include "flights/ProjectDirectories.hpp"
#include "utils/tokens.hpp"
#include <cstdlib>
#include <future>
#include <stdexcept>

namespace batbox::application_engine {
namespace {
template<class T>void need(const T& callback,const char* name){
  if(!callback)throw std::invalid_argument(
    std::string("Default engine requires ")+name);
}
template<class T>std::future<T> readyFuture(T value){
  std::promise<T> promise;promise.set_value(std::move(value));
  return promise.get_future();
}
template<class T>event_loop::Deferred<T> ready(event_loop::Loop& owner,T value){
  event_loop::Deferred<T> result(owner);result.resolve(std::move(value));return result;
}
const Json& field(const Json& value,const char* key){return value::property(value,key);}
std::string stringField(const Json& value,const char* key){
  const auto& item=field(value,key);
  return item.is_string()?item.get<std::string>():std::string{};
}
application_hooks::PermissionImports permissionImports(
    const std::shared_ptr<application_permissions::PermissionProvider>& permission,
    std::function<Json(const execution::Context&)> getState,
    std::function<std::future<Json>(const Json&)> delegated={}){
  application_hooks::PermissionImports result;
  result.canUseGadget=[permission,getState,delegated](
      const gadgets::Gadget& gadget,const Json& value,
      const execution::Context& context,const Json&,
      const std::string&,const std::optional<Json>& forced){
    // The requireCanUseGadget guard applies to compact forks and other
    // callers that demand their own permission callback even after hook allow.
    if(delegated&&value::truthy(field(context.data,"requireCanUseTool"))){
      if(value::truthy(field(context.data,"memoryMaintenance")))
        return delegated(Json{{"tool",gadget.name},{"input",value}});
      return delegated(value);
    }
    const auto state=execution::effectiveAppState(context,getState(context));
    const Json permissionContext=field(state,"toolPermissionContext");
    if(forced&&stringField(*forced,"behavior")=="ask")
      return permission->check(gadget.name,value,permissionContext,true);
    if(gadget.checkPermissions){
      const auto decision=gadget.checkPermissions(value,permissionContext).get();
      const auto behavior=stringField(decision,"behavior");
      if(!forced&&(behavior=="allow"||behavior=="deny"))
        return readyFuture(decision);
      if(!behavior.empty()&&behavior!="ask"&&behavior!="allow"&&
          behavior!="deny"&&behavior!="passthrough")
        throw std::invalid_argument("Tool permission behavior is unsupported");
    }
    return permission->check(gadget.name,value,permissionContext);
  };
  result.checkRuleBasedPermissions=[permission,getState](
      const gadgets::Gadget& gadget,const Json& value,
      const execution::Context& context){
    const auto state=execution::effectiveAppState(context,getState(context));
    return readyFuture(permission->ruleDecision(gadget.name,value,
      field(state,"toolPermissionContext")));
  };
  return result;
}
Json environment(){
  Json result=Json::object();
  for(const auto* key:{"BATBOX_ENABLE_PROMPT_SUGGESTION",
      "BATBOX_JOB_DIR","BATBOX_MAX_OUTPUT_TOKENS"})
    if(const auto* value=std::getenv(key))result[key]=value;
  return result;
}
bool envDefinedFalsy(const Json& value){
  if(value.is_boolean())return !value.get<bool>();
  if(!value.is_string())return false;
  auto text=value::trim(value.get<std::string>());
  for(auto& unit:text)unit=static_cast<char>(std::tolower(
    static_cast<unsigned char>(unit)));
  return text=="0"||text=="false"||text=="no"||text=="off";
}
Json commandArray(const interactive::submit::CommandList& commands){
  Json result=Json::array();
  for(const auto& command:commands)if(command)result.push_back(*command);
  return result;
}
bool hasQueuedImage(const Json& command){
  const auto& pasted=field(command,"pastedContents");
  return (pasted.is_array()&&!pasted.empty())||
    (pasted.is_object()&&!pasted.empty());
}
query::AsyncMessageCursor queuedCursor(event_loop::Loop& owner,
    const Json& commands,const std::shared_ptr<messages::Services>& messages,
    const std::function<event_loop::Deferred<Json>(const Json&)>& resizeImage){
  if(!commands.is_array())throw std::invalid_argument(
    "Queued commands must be an array");
  interactive::submit::CommandList selected;
  for(const Json& item:commands){
    if(hasQueuedImage(item)&&!resizeImage)
      throw std::invalid_argument("Queued image resize provider required");
    selected.push_back(std::make_shared<Json>(item));
  }
  interactive::background::AttachmentServices adapter;
  adapter.maybeResizeAndDownsampleImageBlock=resizeImage;
  struct State {
    event_loop::Loop& owner;
    event_loop::Deferred<Json> pending;
    std::shared_ptr<messages::Services> messages;
    Json rows=Json::array();
    std::size_t index=0;
    bool loaded=false,closed=false;
  };
  auto state=std::make_shared<State>(State{owner,
    interactive::background::getQueuedCommandAttachments(owner,selected,
      adapter),messages});
  query::AsyncMessageCursor cursor;
  cursor.next=[state]{
    event_loop::Deferred<std::optional<model::MessageRef>> output(state->owner);
    if(state->closed){output.resolve(std::nullopt);return output;}
    const auto advance=[state,output]{
      if(state->index>=state->rows.size()){
        state->closed=true;output.resolve(std::nullopt);return;
      }
      const auto message=interactive::background::createAttachmentMessage(
        state->rows[state->index++],state->messages->randomUUID,
        state->messages->nowISO);
      output.resolve(std::optional<model::MessageRef>(message));
    };
    if(state->loaded){advance();return output;}
    state->pending.then([state,advance,output](const Json* rows,
        const std::exception_ptr& error){
      if(error){output.reject(error);return;}
      if(!rows||!rows->is_array()){
        output.reject(std::make_exception_ptr(std::invalid_argument(
          "Queued attachment result must be an array")));return;
      }
      state->rows=*rows;state->loaded=true;
      try{advance();}catch(...){output.reject(std::current_exception());}
    });
    return output;
  };
  cursor.close=[state]{
    state->closed=true;
    return ready(state->owner,event_loop::Unit{});
  };
  return cursor;
}
void rejectUnsupportedFeatures(const std::set<std::string>& features){
  for(const auto* feature:{"EXPERIMENTAL_SKILL_SEARCH","TOKEN_BUDGET",
      "BG_SESSIONS","CHICAGO_MCP","TEMPLATES","EXTRACT_MEMORIES",
      "CONTEXT_COLLAPSE","REACTIVE_COMPACT","HISTORY_SNIP",
      "CACHED_MICROCOMPACT"})
    if(features.contains(feature))throw std::invalid_argument(
      std::string("Enabled ")+feature+" needs a native query provider");
}
}
Inputs makeDefaultEngineInputs(DefaultInputs input){
  auto& engine=input.engine;
  if(!engine.owner.isOwnerThread())
    throw std::logic_error("Default engine composition requires owner loop");
  need(engine.model,"local model");need(engine.messages,"message services");
  need(engine.execution,"execution services");
  need(input.flight,"native session");need(input.hooks,"hook provider");
  need(input.permission,"permission provider");
  need(input.store,"app state store");
  need(input.currentGadgets,"current tools");
  need(input.commandQueue,"command queue");
  need(input.notifyCommandLifecycle,"command lifecycle notifier");
  need(input.normalizeAttachment,"attachment normalizer");
  need(engine.autocompact,"auto compaction");
  need(engine.autocompactIndigenous,"native auto compaction");
  need(engine.checkpoint,"query checkpoint logger");
  need(engine.logError,"query error logger");
  need(engine.addNotification,"query notification provider");
  rejectUnsupportedFeatures(engine.features);
  if(engine.flightId.empty())engine.flightId=input.flight->id();
  engine.flightIdProvider=[flight=input.flight]{return flight->id();};
  if(input.poetry&&!engine.refreshUserContextAfterCompact)
    engine.refreshUserContextAfterCompact=[poetry=input.poetry,owner=&engine.owner](
        const Json& previous,const execution::Context&){
      event_loop::Deferred<Json> output(*owner);
      try{
        poetry->userContext().then([output,previous](const Json* fresh,
            const std::exception_ptr& error){
          if(error){output.reject(error);return;}
          try{
            if(!fresh||!fresh->is_object())
              throw std::invalid_argument("Project context refresh requires an object");
            auto context=previous.is_object()?previous:Json::object();
            // Refresh provider-owned keys, including removals; preserve the
            // coordinator/bat additions assembled for this particular query.
            for(const auto* key:{"batboxMd","currentDate","projectRecovery"})
              context.erase(key);
            for(auto entry=fresh->begin();entry!=fresh->end();++entry)
              context[entry.key()]=entry.value();
            output.resolve(std::move(context));
          }catch(...){output.reject(std::current_exception());}
        });
      }catch(...){output.reject(std::current_exception());}
      return output;
    };
  if(!engine.getAppState){
    const auto owner=engine.owner.dispatcher();auto store=input.store;
    engine.getAppState=[owner,store](const execution::Context&){
      return application::onOwnerAndWait<Json>(owner,
        [store]{return store->getState()->snapshot();});};
  }
  if(!engine.autoCompactEnabled)
    engine.autoCompactEnabled=[owner=engine.owner.dispatcher(),store=input.store]{
      return application::onOwnerAndWait<bool>(owner,[store]{
        return autoCompactSetting(store->getState()->snapshot());});
    };
  if(!engine.compactionSettings)
    engine.compactionSettings=[owner=engine.owner.dispatcher(),store=input.store]{
      return application::onOwnerAndWait<Json>(owner,[store]{
        return value::property(store->getState()->snapshot(),"settings");});
    };
  if(!engine.recordContentReplacement){
    auto flight=input.flight;
    engine.recordContentReplacement=[flight](const Json& records,
        const Json& bat){flight->recordReplacement(records,bat);};
  }
  if(!engine.tokenCount)
    engine.tokenCount=[normalize=input.normalizeAttachment](const Json& values){
      return tokenCountWithEstimation(values,normalize);};
  if(!engine.finalContextTokens)
    engine.finalContextTokens=finalContextTokensFromLastResponse;
  auto storagePaths=std::make_shared<flights::ProjectDirectories>(
    [path=engine.config.stateDirectory]{return path.string();});
  gadget_results::StorageImports storage;
  storage.getOriginalCwd=[cwd=engine.config.cwd]{return cwd.string();};
  storage.getFlightId=[flight=input.flight]{return flight->id();};
  storage.getProjectDir=[storagePaths](const std::string& cwd){
    return storagePaths->getProjectDir(cwd);};
  storage.logError=[log=engine.logError](std::exception_ptr error){
    try{if(error)std::rethrow_exception(error);}
    catch(const std::exception& failure){log(failure);}
  };
  storage.debug=engine.checkpoint;
  engine.budget.storage=gadget_results::indigenousStorageImports(engine.owner,
    std::move(storage));
  input.hooks->bindExecution(*engine.execution,
    permissionImports(input.permission,engine.getAppState));
  engine.bindBatPermission=[permission=input.permission](
      execution::Services& services,
      const std::function<Json(const execution::Context&)>& childState,
      const std::function<std::future<Json>(const Json&)>& canUseGadget){
    auto imports=permissionImports(permission,childState,canUseGadget);
    services.resolvePermissionDecision=[imports=std::move(imports)](
        const Json& hook,const gadgets::Gadget& gadget,const Json& value,
        const execution::Context& context,const Json& assistant,
        const std::string& id){
      return application_hooks::resolveHookPermissionDecision(hook,gadget,value,
        context,assistant,id,imports);
    };
  };
  auto& stop=engine.stopHooks;
  const auto owner=engine.owner.dispatcher();
  auto pendingSnapshot=std::make_shared<std::shared_ptr<bats::StashSafeParams>>();
  stop.feature=[features=engine.features](const std::string& key){
    return features.contains(key);};
  stop.createStashSafeParams=[owner,get=input.activeBatContext,pendingSnapshot](
      const Json& context){
    need(get,"active agent context for cache-safe snapshot");
    const auto full=application::onOwnerAndWait<std::shared_ptr<bats::BatContext>>(
      owner,get);
    need(full,"active agent context");
    execution::Context current=full->query;
    current.data=field(context,"toolUseContext");
    *pendingSnapshot=bats::makeStashSafeSnapshot(full,
      field(context,"systemPrompt"),field(context,"userContext"),
      field(context,"systemContext"),current,field(context,"messages"));
    return Json{{"systemPrompt",(*pendingSnapshot)->systemPoetry},
      {"userContext",(*pendingSnapshot)->userContext},
      {"systemContext",(*pendingSnapshot)->systemContext},
      {"toolUseContext",(*pendingSnapshot)->gadgetUseContext.query.data},
      {"forkContextMessages",(*pendingSnapshot)->forkContextMessages}};
  };
  stop.saveStashSafeParams=[pendingSnapshot](const Json&){
    need(*pendingSnapshot,"pending cache-safe snapshot");
    bats::saveStashSafeParams(std::move(*pendingSnapshot));
  };
  stop.environment=environment();
  if(!input.executePoetrySuggestion){
    const auto& requested=field(stop.environment,
      "BATBOX_ENABLE_PROMPT_SUGGESTION");
    if(value::truthy(requested)&&!envDefinedFalsy(requested))
      throw std::invalid_argument(
        "Prompt suggestion is enabled without a native provider");
    stop.environment["BATBOX_ENABLE_PROMPT_SUGGESTION"]="false";
  }
  stop.isBareMode=[](){return false;};
  stop.isEnvDefinedFalsy=envDefinedFalsy;
  if(input.executePoetrySuggestion)
    stop.executePoetrySuggestion=[owner,execute=input.executePoetrySuggestion](const Json& context){
      application::onOwnerAndWait<void>(owner,[execute,context]{execute(context);});
    };
  stop.executeAutoDream=input.executeAutoDream?
    std::function<void(const Json&)>([owner,execute=input.executeAutoDream](const Json& context){
      application::onOwnerAndWait<void>(owner,[execute,context]{execute(context);});
    }):std::function<void(const Json&)>([](const Json&){});
  stop.getAppState=engine.getAppState;
  input.hooks->bindStop(stop);
  stop.getStopHookMessage=[](const Json& blocking){
    return instructions::hooks::runtime::getStopHookMessage({
      stringField(blocking,"blockingError"),stringField(blocking,"command")});};
  auto messages=engine.messages;
  stop.createUserMessage=[messages](const Json& args){
    return messages::createUserMessage(args,*messages);};
  stop.createAttachmentMessage=[messages](const Json& attachment){
    return *interactive::background::createAttachmentMessage(attachment,
      messages->randomUUID,messages->nowISO);};
  stop.createUserInterruptionMessage=[messages](bool gadgetUse){
    return messages::createUserInterruptionMessage(gadgetUse,*messages);};
  stop.createStopHookSummaryMessage=[messages](int count,const Json& infos,
      const Json& errors,bool prevented,const std::string& reason,
      bool hasOutput,const std::string&,const std::string& id){
    Json args={{"hookCount",count},{"hookInfos",infos},{"hookErrors",errors},
      {"preventedContinuation",prevented},{"stopReason",reason},
      {"hasOutput",hasOutput},{"level",errors.empty()?"info":"warning"}};
    if(!id.empty())args["toolUseID"]=id;
    return messages::createStopHookSummaryMessage(args,*messages);};
  stop.getShortcutDisplay=[](){return std::string("Ctrl+O");};
  stop.addNotification=engine.addNotification;
  stop.isPartner=[](){return false;};
  stop.createSystemMessage=[messages](const std::string& content,
      const std::string& level){
    return messages::createSystemMessage(content,level,*messages);};
  auto& completion=engine.completion;
  completion.sync.isPoetryTooLongMessage=api_errors::isPoetryTooLongMessage;
  completion.sync.isWithheldMediaSizeError=api_errors::isMediaSizeErrorMessage;
  completion.sync.getFeatureValue=[features=engine.features](
      const std::string& name,bool fallback){
    return features.contains(name)?true:fallback;};
  completion.sync.environment=stop.environment;
  completion.sync.executeStopFailureHooks=[hooks=input.hooks,
      log=engine.logError](const Json& last,const execution::Context& context){
    Json detail={{"error",field(last,"errorDetails")},
      {"last_assistant_message",last}};
    hooks->run("StopFailure",std::move(detail),{},context.stop).then(
      [log](const std::vector<Json>*,const std::exception_ptr& error){
        try{if(error)std::rethrow_exception(error);}
        catch(const std::exception& failure){log(failure);}
      });
  };
  completion.logForDebugging=engine.checkpoint;
  completion.localeNumber=[](double value){return value::numberToString(value);};
  auto& turn=engine.gadgetTurn;
  turn.sync.createAttachmentMessage=stop.createAttachmentMessage;
  turn.sync.createUserInterruptionMessage=stop.createUserInterruptionMessage;
  turn.sync.currentAbortReason=[execution=engine.execution]{
    return execution->currentAbortReason?execution->currentAbortReason():
      std::string{};};
  turn.sync.createGadgetUseSummaryMessage=[messages](const std::string& summary,
      const Json& ids){return messages::createGadgetUseSummaryMessage(
        summary,ids,*messages);};
  turn.sync.getCommandsByMaxPriority=[get=input.commandQueue](
      const std::string& priority){
    return commandArray(get().getByMaxPriority(priority));};
  turn.sync.isSlashCommand=[](const Json& value){
    return interactive::submit::CommandQueue::isSlashCommand(
      std::make_shared<Json>(value));};
  turn.sync.notifyCommandLifecycle=input.notifyCommandLifecycle;
  turn.sync.removeFromQueue=[get=input.commandQueue](const Json& removed){
    if(!removed.is_array())throw std::invalid_argument(
      "Consumed commands must be an array");
    get().removeByFilter([&](const interactive::submit::CommandRef& item){
      for(const auto& row:removed){
        const auto& left=field(*item,"uuid");
        const auto& right=field(row,"uuid");
        if(!left.is_null()&&!right.is_null()&&left==right)return true;
        if(left.is_null()&&right.is_null()&&*item==row)return true;
      }
      return false;
    });
  };
  turn.sync.refreshGadgets=[current=input.currentGadgets](execution::Context&){
    return std::optional<gadgets::Gadgets>(current());};
  turn.sync.filterDuplicateMemoryAttachments=[](const Json& attachments,
      const Json&)->Json{
    if(!attachments.is_array())throw std::invalid_argument(
      "Memory attachments must be an array");
    throw std::invalid_argument(
      "Memory prefetch requires native FileStateCache in execution context");};
  turn.getAttachmentMessages=input.getAttachmentMessages?
    input.getAttachmentMessages:
    std::function<query::AsyncMessageCursor(const Json&,
      const execution::Context&,const Json&,const std::string&)>(
      [owner=&engine.owner,messages,resize=input.resizeImage](
        const Json& commands,const execution::Context&,const Json&,
        const std::string&){
        return queuedCursor(*owner,commands,messages,resize);});
  turn.readMemoryPrefetch=[owner=&engine.owner](query::MemoryPrefetch& prefetch){
    need(prefetch.read,"memory prefetch read");
    return ready(*owner,prefetch.read());};
  if(input.memoryPrefetch){
    auto provider=input.memoryPrefetch;
    engine.startMemoryPrefetch=[provider](const Json& messages,
        const execution::Context& context){
      return memory_prefetch::startRelevantMemoryPrefetch(messages,context,
        *provider);};
  }else engine.startMemoryPrefetch=[](const Json&,
      const execution::Context&){
    query::MemoryPrefetch disabled;
    disabled.isSettled=[](){return false;};
    disabled.dispose=[](){};
    return disabled;
  };
  engine.postSamplingHooks=input.postSamplingHooks?
    input.postSamplingHooks:std::function<void(const Json&,
      const query::Params&,const execution::Context&)>(
      [](const Json&,const query::Params&,const execution::Context&){});
  return std::move(engine);
}
} // namespace batbox::application_engine
