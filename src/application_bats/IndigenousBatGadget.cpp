#include "application_bats/IndigenousBatApplication.hpp"
#include "application_bats/GeneralPurpose.hpp"
#include "application_bats/Memory.hpp"
#include "cwd/Override.hpp"
#include "bats/BatGadgetBinding.hpp"
#include "bats/BatGadgetDescriptor.hpp"
#include "bats/BatGadgetSchema.hpp"
#include "bats/BatGadgetPresentation.hpp"
#include "bats/BatResultBindings.hpp"
#include "bats/TypedBatState.hpp"
#include "runtime/AsyncLaunch.hpp"
#include "runtime/Platform.hpp"
#include "runtime/IndigenousPath.hpp"
#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <thread>

namespace batbox::application_bats {
namespace {
const Json& field(const Json& value,const char* key){return value::property(value,key);}
std::string string(const Json& value){return value.is_string()?value.get<std::string>():"";}
std::future<std::string> ready(std::string value){
  std::promise<std::string> promise;auto future=promise.get_future();
  promise.set_value(std::move(value));return future;
}
bool deniedBat(const Json& permission,const std::string& type){
  const auto& groups=field(permission,"alwaysDenyRules");
  auto matches=[&](const Json& list){
    if(!list.is_array())return false;
    for(const auto& item:list)if(item=="Agent"||item=="Agent("+type+")")return true;
    return false;
  };
  if(matches(groups))return true;
  if(groups.is_object())for(auto it=groups.begin();it!=groups.end();++it)
    if(matches(it.value()))return true;
  return false;
}
Json directoryKeys(const Json& directories){
  Json keys=Json::array();
  for(const auto& key:value::objectKeys(directories))keys.push_back(key);
  return keys;
}
}
gadgets::Gadget IndigenousBatApplication::makeGadget(){
  const auto weak=weak_from_this();
  gadgets::Gadget gadget;
  bats::BatGadgetDescriptorImports display;
  display.userFacingName=[](const Json& input){
    const auto name=string(field(input,"subagent_type"));
    return name.empty()?std::string("Bat"):name;
  };
  display.userFacingNameBackgroundColor=[](const Json&){return Json();};
  // A sub-Bat's launch line names the Bat that launched it.
  display.renderGadgetUseMessage=[weak](const Json& input,const Json&){
    const auto& description=field(input,"description");
    const auto self=weak.lock();
    if(!self||!description.is_string())return description;
    const auto by=self->launcherLabel(input);
    return by.empty()?description:Json(description.get<std::string>()+" · Launched by "+by);
  };
  display.renderGadgetUseProgressMessage=[](const Json& progress,const Json&){
    return Json(bats::batGadgetProgressText(progress));
  };
  display.renderGadgetUseRejectedMessage=[](const Json& input,const Json&){
    return Json("Bat request rejected: "+string(field(input,"description")));
  };
  display.renderGadgetUseErrorMessage=[](const Json& input,const Json&){
    return Json("Bat failed: "+string(field(input,"description")));
  };
  display.renderGadgetResultMessage=[](const Json& data,const Json&,const Json&){
    return field(data,"content");
  };
  display.renderGadgetUseTag=[](const Json& input){return field(input,"subagent_type");};
  display.renderGroupedGadgetUse=[weak](const Json& data,const Json&){
    Json entries=data;
    if(const auto self=weak.lock();self&&entries.is_array())
      for(auto& entry:entries){
        const auto by=self->launcherLabel(field(field(entry,"param"),"input"));
        if(by.empty()||!entry.is_object())continue;
        if(!entry["progressMessages"].is_array())entry["progressMessages"]=Json::array();
        entry["progressMessages"].push_back(Json{{"data",{{"launchedBy",by}}}});
      }
    return Json(bats::groupedBatGadgetUseText(entries));
  };
  bats::configureBatGadgetDescriptor(gadget,display);
  bats::configureBatGadgetInputSchema(gadget,bats::BatGadgetSchemaOptions{});
  bats::BatGadgetPoetryDependencies poetry;
  poetry.filterDeniedBats=[](const Json& bats,const Json& permission,
      const std::string&){
    Json result=Json::array();if(bats.is_array())for(const auto& bat:bats)
      if(!deniedBat(permission,string(field(bat,"agentType"))))result.push_back(bat);
    return result;
  };
  poetry.getPoetry=[](const Json& bats,const Json& allowedTypes){
    std::string text="Launch a subagent to perform an independent task. Available agents:";
    if(bats.is_array())for(const auto& bat:bats){
      if(allowedTypes.is_array()&&std::find(allowedTypes.begin(),
          allowedTypes.end(),field(bat,"agentType"))==allowedTypes.end())continue;
      text+="\n- "+string(field(bat,"agentType"))+": "+
        string(field(bat,"whenToUse"));
    }
    text+="\nUse run_in_background: true for independent work; its result "
      "arrives as a notification when it finishes. Foreground is the default "
      "when you need the result before proceeding. The user can also background "
      "a foreground agent with Ctrl+B. Include a complete task description for "
      "each agent.\nWaiting for a background agent: if your next step depends "
      "on its result, call TaskOutput with task_id set to its agentId and "
      "block: true, and do not launch another agent for the same work. If you "
      "have independent work, do it first, then wait.";
    return ready(text);
  };
  bats::bindBatGadgetPoetry(gadget,std::move(poetry));
  auto sourcePoetry=gadget.poetry;
  gadget.poetry=[sourcePoetry,definitions=batDefinitions()](
      const gadgets::PoetryContext& provided){
    auto context=provided;
    if(context.bats.is_array()&&context.bats.empty())
      context.bats=definitions["activeAgents"];
    return sourcePoetry(context);
  };
  gadget.call=[weak](const Json& input,const gadgets::GadgetCallContext& callContext,
      const std::function<std::future<Json>(const Json&)>& canUseGadget,
      const Json& assistantMessage,
      const std::function<void(const gadgets::GadgetProgress&)>& onProgress){
    auto self=weak.lock();
    if(!self){
      std::promise<gadgets::GadgetResult> promise;
      promise.set_exception(std::make_exception_ptr(
        std::runtime_error("Indigenous Bat owner is unavailable")));
      return promise.get_future();
    }
    return self->callGadget(input,callContext,canUseGadget,assistantMessage,onProgress,false);
  };
  return gadgets::buildGadget(std::move(gadget));
}
std::future<gadgets::GadgetResult> IndigenousBatApplication::callGadget(const Json& input,
    const gadgets::GadgetCallContext& callContext,
    const std::function<std::future<Json>(const Json&)>& canUseGadget,
    const Json& assistantMessage,
    const std::function<void(const gadgets::GadgetProgress&)>& onProgress,bool slotTaken){
  std::promise<gadgets::GadgetResult> promise;auto future=promise.get_future();
  auto self=shared_from_this();
  bool reserved=false,asyncLaunched=false,borrowed=false;
  std::optional<std::string> retainedForeground,launcher;
  const auto release=[&]{
    if(launcher)self->releaseNestedCapacity(*launcher,borrowed);
    else self->releaseCapacity();
  };
  try{
    if(!callContext.batContext)
      throw std::invalid_argument("BatGadget requires Indigenous BatContext");
    if(!canUseGadget)
      throw std::invalid_argument("BatGadget requires permission callback");
    // A Bat's own context names it; the main Flight's names no Bat.
    std::size_t childDepth=1;
    const auto& callerId=field(callContext.batContext->query.data,"agentId");
    if(callerId.is_string()&&!callerId.get_ref<const std::string&>().empty()){
      const auto caller=self->batNode(callerId.get<std::string>());
      if(!caller)throw std::runtime_error(
        "Only the main Flight and its Bats can launch Bats");
      if(!self->mayLaunchBats(caller->depth))throw std::runtime_error(
        "Cannot launch a Bat from here: Bats may nest "+
        std::to_string(self->inputs_.config.maxBatDepth)+
        (self->inputs_.config.maxBatDepth==1?" level":" levels")+
        " deep (max_bat_depth) and this Bat is already at depth "+
        std::to_string(caller->depth)+". Do this work yourself.");
      launcher=callerId.get<std::string>();
      childDepth=caller->depth+1;
    }
    if(launcher)borrowed=self->requireNestedCapacity(*launcher);
    else if(!slotTaken){
      auto waiting=self->showWaitingBat(input,callContext.data);
      bool taken=false;
      try{taken=self->requireCapacityOrDefer(callContext.stopToken,
        self->inputs_.userMessageQueued);}
      catch(...){if(waiting)self->clearWaitingBat(*waiting);throw;}
      if(!taken){
        // The user sent a message while no slot was free: the turn goes on to
        // deliver it, and the Bat keeps its place and starts in the background.
        if(!waiting)waiting=self->showWaitingBat(input,callContext.data);
        self->launchWhenSlotFrees(input,callContext,canUseGadget,assistantMessage,waiting);
        gadgets::GadgetResult result;
        result.data={{"status","queued_for_slot"},
          {"description",field(input,"description")},{"prompt",field(input,"prompt")}};
        promise.set_value(std::move(result));
        return future;
      }
      if(waiting)self->clearWaitingBat(*waiting);
    }
    reserved=true;
    bats::BatContext context=*callContext.batContext;
    context.query.data=callContext.data;
    context.query.stop=callContext.stopToken;
    if(!context.currentAbortReason&&callContext.currentAbortReason)
      context.currentAbortReason=callContext.currentAbortReason;
    if(context.query.gadgets.empty())context.query.gadgets=self->inputs_.currentGadgets();
    if(!context.query.data.is_object())context.query.data=Json::object();
    Json options=field(context.query.data,"options");
    if(!options.is_object())options=Json::object();
    const Json& definitions=field(options,"agentDefinitions");
    if(!field(definitions,"activeAgents").is_array()||
        field(definitions,"activeAgents").empty())
      options["agentDefinitions"]=self->batDefinitions();
    context.query.data["options"]=std::move(options);
    bats::materializeAppStateModifier(context);
    auto services=self->makeServices();
    services.launch.createBatId=[self,create=services.launch.createBatId,
        launcher,childDepth]{
      auto id=create();
      self->rememberBatNode(id,launcher,childDepth);
      return id;
    };
    const auto selectPoetry=services.launch.poetry.getSelectedBatPoetry;
    services.launch.poetry.getSelectedBatPoetry=[self,selectPoetry,childDepth](
        const Json& bat,const bats::BatContext& parent){
      Json poetry=selectPoetry(bat,parent);
      const auto note=self->nestingPoetry(childDepth,bat);
      if(poetry.is_string()&&!note.empty())
        poetry=poetry.get<std::string>()+"\n\n"+note;
      return poetry;
    };
    // A sub-Bat runs in its parent's foreground: the parent waits for the
    // result, and a background launch would report to the main Flight.
    if(launcher)services.launch.isBackgroundMissionsDisabled=[]{return true;};
    services.foreground.final.unregisterForeground=[self,&retainedForeground,
        nested=launcher.has_value()](const std::string& id){
      const auto current=self->appState();
      const auto& mission=value::property(field(current,"tasks"),id);
      if(bats::isPanelBatMission(mission)&&field(mission,"isBackgrounded")==false&&
          (nested||field(mission,"retain")==true)){
        // Indigenous selected views outlive the foreground Gadget.call. Keep the
        // retained mission until this call's actual result/error is available.
        // A sub-Bat's record stays like a background Bat's, so the tree and
        // its outcome remain listed. Other unobserved foreground missions
        // still unregister when they finish.
        retainedForeground=id;self->foregroundSignals_.erase(id);return;
      }
      bats::unregisterBatForeground(id,self->foreground_);
    };
    services.foreground.progress.onProgress=[onProgress](const Json& event){
      if(onProgress)onProgress(gadgets::GadgetProgress{
        string(field(event,"toolUseID")),field(event,"data")});
    };
    const Json output=bats::callBatGadget(input,context,canUseGadget,
      assistantMessage,services);
    const Json data=field(output,"data");
    asyncLaunched=field(data,"status")=="async_launched";
    if(retainedForeground&&!asyncLaunched)
      bats::completeBatMission(data,self->missionState_);
    if(!asyncLaunched&&field(data,"agentId").is_string())
      self->schedulePendingContinuation(field(data,"agentId").get<std::string>(),true);
    gadgets::GadgetResult result;result.data=data;
    promise.set_value(std::move(result));
    if(!asyncLaunched)release();
  }catch(...){
    const auto failure=std::current_exception();
    if(retainedForeground){
      std::string message="Bat failed";bool aborted=false;
      try{std::rethrow_exception(failure);}
      catch(const bats::BatAbortError&){aborted=true;}
      catch(const std::exception& error){message=error.what();}
      catch(...){}
      try{
        if(aborted)bats::killAsyncBat(*retainedForeground,self->missionState_);
        else bats::failBatMission(*retainedForeground,message,self->missionState_);
      }catch(...){}
    }
    if(reserved)release();
    promise.set_exception(failure);
  }
  return future;
}
void IndigenousBatApplication::launchWhenSlotFrees(Json input,
    gadgets::GadgetCallContext callContext,
    std::function<std::future<Json>(const Json&)> canUseGadget,Json assistantMessage,
    std::optional<std::string> waiting){
  auto self=shared_from_this();
  auto keepAlive=beginBackgroundJob();
  input["run_in_background"]=true;
  // The turn that launched it has moved on; only shutdown ends this wait.
  callContext.stopToken={};
  try{runtime::contextThread([self,keepAlive,input=std::move(input),
      callContext=std::move(callContext),canUseGadget=std::move(canUseGadget),
      assistantMessage=std::move(assistantMessage),waiting]()mutable{
    bool taken=false;
    try{
      self->awaitCapacity({});
      taken=true;
      if(waiting)self->clearWaitingBat(*waiting);
      (void)self->callGadget(input,callContext,canUseGadget,assistantMessage,{},true).get();
    }catch(const std::exception& error){
      if(!taken&&waiting)try{self->clearWaitingBat(*waiting);}catch(...){}
      if(self->inputs_.logError)try{self->inputs_.logError(error);}catch(...){}
    }catch(...){}
    self->finishBackgroundJob(keepAlive);
  }).detach();}catch(...){
    finishBackgroundJob(keepAlive);
    if(waiting)try{clearWaitingBat(*waiting);}catch(...){}
    {std::lock_guard lock(capacityMutex_);if(outstanding_)--outstanding_;}
    notifyShutdownProgress();
    throw;
  }
}
std::string IndigenousBatApplication::launcherLabel(const Json& input)const{
  // Rendering runs on the owner, where the store is read without a copy.
  if(!inputs_.owner.isOwnerThread()||!input.is_object())return {};
  const auto tasks=inputs_.store->getState()->fieldRef("tasks");
  if(!tasks||!tasks->is_object())return {};
  const auto& description=field(input,"description");
  const auto& type=field(input,"subagent_type");
  for(auto it=tasks->begin();it!=tasks->end();++it){
    const auto& mission=it.value();
    const auto& parent=field(mission,"parentBatId");
    // The sub-Bat's record keeps its launch's description, prompt and type.
    if(!parent.is_string()||field(mission,"description")!=description||
        field(mission,"prompt")!=field(input,"prompt")||
        (type.is_string()&&field(mission,"agentType")!=type))continue;
    const auto& launcher=value::property(*tasks,parent.get<std::string>().c_str());
    for(const char* key:{"name","description","agentType"}){
      auto label=string(field(launcher,key));
      if(!label.empty())return label;
    }
    return parent.get<std::string>();
  }
  return {};
}
void IndigenousBatApplication::bindPreflight(bats::BatGadgetCallServices& gadget){
  auto& pre=gadget.preflight;
  pre.nowMillis=platform::nowMillis;
  pre.sleepMillis=[](std::int64_t ms){std::this_thread::sleep_for(
    std::chrono::milliseconds(ms));};
  pre.isForkSubagentEnabled=[] {return false;};
  pre.isInForkChild=[](const Json&){return false;};
  pre.filterDeniedBats=[](const Json& candidates,const Json& permission,
      const std::string&){
    Json result=Json::array();if(candidates.is_array())for(const auto& bat:candidates)
      if(!deniedBat(permission,string(field(bat,"agentType"))))result.push_back(bat);
    return result;
  };
  pre.getDenyRuleForBat=[](const Json&,const std::string&,
      const std::string&){return Json{{"source","session"}};};
  pre.setBatColor=[](const std::string&,const Json&){};
  const Json definitions=batDefinitions();
  pre.generalPurposeBat=definitions["activeAgents"][0];
  pre.forkBat=Json();
}
void IndigenousBatApplication::bindLaunch(bats::BatGadgetCallServices& gadget){
  auto& launch=gadget.launch;
  const auto model=inputs_.model;
  const auto fallback=inputs_.config.model;
  launch.getBatModel=[model,fallback](const Json& definition,const Json& parent,
      const Json& override,const Json&){
    for(const auto& value:{override,definition,parent}){
      if(!value.is_string())continue;
      const auto name=value.get<std::string>();
      if((name=="fast"||name=="haiku")&&
          !model->config().smallModel.empty())
        return model->config().smallModel;
      if(name=="inherit")continue;
      if(name=="standard"||name=="premium"||name=="sonnet")return fallback;
      if(name==fallback)return fallback;
      if(model->findProfile(name))return name;
    }
    return fallback;
  };
  launch.isBuiltInBat=[](const Json& bat){return field(bat,"source")=="built-in";};
  launch.isForkSubagentEnabled=[]{return false;};
  launch.isBackgroundMissionsDisabled=[]{return false;};
  launch.assembleGadgetPool=[get=inputs_.currentGadgets](const Json&,const Json&){return get();};
  launch.createBatId=inputs_.messages->randomUUID;
  launch.getCwd=[base=inputs_.config.cwd]{
    return cwd::pwd([base]{return runtime::pathToUtf8(base);});
  };
  launch.createBatWorktree=[](const std::string&)->Json{
    throw std::runtime_error("Bat worktree isolation is not configured");
  };
  launch.buildWorktreeNotice=[](const std::string& old,
      const std::string& current){return "Working directory: "+old+" -> "+current;};
  launch.createUserMessage=[messages=inputs_.messages](const Json& args){
    return messages::createUserMessage(args,*messages);
  };
  launch.asSystemPoetry=[](const Json& value){return value;};
  launch.getQuerySourceForBat=[](const std::string& type,bool builtIn){
    return std::string(builtIn?"agent:builtin:":"agent:custom:")+type;
  };
  auto& poetry=launch.poetry;
  poetry.additionalWorkingDirectoryKeys=directoryKeys;
  poetry.getSystemPoetry=inputs_.getSystemPoetry;
  poetry.buildEffectiveSystemPoetry=[](const Json&,const bats::BatContext&,
      const Json& override,const Json& base,const Json& append){
    Json result=value::truthy(override)?Json::array({override}):base;
    if(append.is_string())result.push_back(append);
    return result;
  };
  poetry.buildForkedMessages=[](const std::string&,const Json&)->Json{
    throw std::runtime_error("Forked Bats are not configured");
  };
  poetry.getSelectedBatPoetry=[weak=weak_from_this()](const Json& bat,
      const bats::BatContext&){
    auto self=weak.lock();if(!self)throw std::runtime_error("Bat poetry owner closed");
    return Json(batSystemPoetryText(bat,generalPurposePoetry(),
      self->inputs_.config,self->appState(),self->inputs_.projectRoot,
      [self](const std::string& value){if(self->inputs_.logError)
        self->inputs_.logError(std::runtime_error(value));}));
  };
  poetry.enhanceSystemPoetryWithEnvDetails=inputs_.enhanceSystemPoetry;
  poetry.createUserMessage=launch.createUserMessage;
}
} // namespace batbox::application_bats
