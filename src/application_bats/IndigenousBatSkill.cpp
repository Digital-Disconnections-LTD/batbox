#include "application_bats/IndigenousBatApplication.hpp"
#include "application/Async.hpp"
#include "bats/AsyncLifecycle.hpp"
#include "bats/BatMissionRegistration.hpp"
#include "bats/RunBat.hpp"
#include "mcp/manager/IndigenousRuntime.hpp"
#include "runtime/AsyncLaunch.hpp"
#include "runtime/Platform.hpp"
#include <stdexcept>

namespace batbox::application_bats {
namespace {
const Json& field(const Json& value,const char* key){return value::property(value,key);}
std::string string(const Json& value){return value.is_string()?value.get<std::string>():"";}
}
void IndigenousBatApplication::runForSkill(const bats::RunBatParams& params,
    const query::Yield& yield){
  requireCapacity(params.gadgetUseContext.query.stop);
  try{
    auto preflight=makeRunPreflight();
    auto setup=makeRunSetup();
    auto loop=makeRunLoop();
    const auto bridge=query_;
    loop.runIndigenousAsyncQuery=[bridge](const query::Params& request,
        const bats::BatContext& context,
        const IndigenousQueryBridge::CanUseGadget& canUse,
        const query::Yield& sink){bridge->run(request,context,canUse,sink);};
    bats::runBat(params,preflight,setup,loop,yield);
  }catch(...){releaseCapacity();throw;}
  releaseCapacity();
}
event_loop::Deferred<event_loop::Unit> IndigenousBatApplication::launchSkillBat(
    std::string description,SkillBatPrepare prepare){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("Skill Bat launch requires owner");
  event_loop::Deferred<event_loop::Unit> output(inputs_.owner);
  try{
    if(closing_)throw std::runtime_error("Agent application is shutting down");
    if(!prepare)throw std::invalid_argument("Skill Bat launch requires a run");
    if(!inputs_.currentBatContext)throw std::runtime_error("The Flight context is unavailable");
    // Outside a turn the last turn's context is stale: its stop token may be
    // spent and its state callbacks are that turn's copies. Start from live
    // ones, as resuming a Bat does.
    auto context=inputs_.currentBatContext();
    const IndigenousQueryBridge::CanUseGadget canUse=[weak=weak_from_this()](const Json& input){
      const auto self=weak.lock();
      if(!self){
        std::promise<Json> failed;auto future=failed.get_future();
        failed.set_exception(std::make_exception_ptr(std::runtime_error("Native Agent owner is unavailable")));
        return future;
      }
      return self->inputs_.permissions->check("Agent",input,
        field(self->appState(),"toolPermissionContext"));
    };
    context.getAppState=[weak=weak_from_this()]{
      auto self=weak.lock();if(!self)throw std::runtime_error("Agent owner closed");return self->appState();
    };
    context.setAppState=missionState_.setAppState;
    context.setAppStateForMissions=missionState_.setAppState;
    const auto owner=inputs_.owner.dispatcher();
    context.getTypedAppState=[owner,store=inputs_.store]{
      return application::onOwnerAndWait<app_state::AppStateRef>(owner,[store]{return store->getState();});
    };
    context.setTypedAppState=[owner,store=inputs_.store](const app_state::AppStateUpdater& update){
      application::onOwnerAndWait<void>(owner,[store,update]{store->setState(update);});
    };
    context.setTypedAppStateForMissions=context.setTypedAppState;
    context.abortOwner=std::make_shared<std::stop_source>();
    context.query.stop=context.abortOwner->get_token();
    context.query.abortReason.clear();
    context.currentAbortReason=[]{return std::string{};};
    context.query.gadgets=inputs_.currentGadgets();
    if(inputs_.currentGadgetOwner)context.query.gadgetSnapshotOwner=inputs_.currentGadgetOwner();
    if(!context.query.data.is_object())context.query.data=Json::object();
    context.query.data["options"]["agentDefinitions"]=batDefinitions();
    context.query.data["options"]["mainLoopModel"]=inputs_.config.model;
    const auto mcp=inputs_.batMcpRuntime?inputs_.batMcpRuntime():nullptr;
    context.query.data["options"]["mcpClients"]=mcp?mcp->manager()->clients():
      field(field(appState(),"mcp"),"clients");
    context.query.data.erase("agentId");
    context.query.data.erase("toolUseId");
    context.query.data["messages"]=Json::array();
    bats::materializeAppStateModifier(context);
    auto self=shared_from_this();
    application::runInWorker<event_loop::Unit>(inputs_.owner,
      [self,prepare=std::move(prepare),context=std::move(context),canUse,output,
        description]()mutable{
        bats::RunBatParams params;
        prepare(context,canUse,params);
        // Hand the run back to the owner, which takes or waits for its slot.
        application::onOwnerAndWait<void>(self->inputs_.owner.dispatcher(),
          [self,&params,&description]{
            self->startSkillBat(description,std::move(params));});
        return event_loop::Unit{};
      }).then([output](const event_loop::Unit*,const std::exception_ptr& error){
        if(error)output.reject(error);else output.resolve({});
      });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
void IndigenousBatApplication::startSkillBat(const std::string& description,
    bats::RunBatParams params){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("Skill Bat start requires owner");
  // Counts the launch, and takes a slot when one is free without waiting.
  const bool taken=requireCapacityOrDefer({},[]{return true;});
  if(taken){runSkillBat(description,std::move(params));return;}
  const Json input={{"description",description},
    {"subagent_type",field(params.batDefinition,"agentType")}};
  std::optional<std::string> waiting;
  std::shared_ptr<std::stop_source> cancel;
  std::shared_ptr<event_loop::Loop::KeepAlive> keepAlive;
  try{
    waiting=showWaitingBat(input,Json::object());
    if(waiting)cancel=watchWaitingBat(*waiting);
    keepAlive=beginBackgroundJob();
  }catch(...){
    if(waiting){(void)forgetWaitingBat(*waiting);try{clearWaitingBat(*waiting);}catch(...){}}
    {std::lock_guard lock(capacityMutex_);if(outstanding_)--outstanding_;}
    notifyShutdownProgress();
    throw;
  }
  // Only x on the waiting row or shutdown ends this wait.
  const auto stop=cancel?cancel->get_token():std::stop_token{};
  auto self=shared_from_this();
  try{runtime::contextThread([self,keepAlive,description,params=std::move(params),
      waiting,stop,input]()mutable{
    try{
      bool taken=false,cancelled=false;
      try{self->awaitCapacity(stop);taken=true;}
      catch(...){
        cancelled=waiting&&self->forgetWaitingBat(*waiting);
        if(!cancelled)throw;
      }
      if(taken&&waiting&&self->forgetWaitingBat(*waiting)){
        // Cancelled just as the slot came free: give the slot back.
        self->releaseCapacity();
        cancelled=true;
      }
      if(waiting)self->clearWaitingBat(*waiting);
      if(cancelled)self->reportCancelledBeforeStart(*waiting,input,Json::object());
      else self->runSkillBat(description,std::move(params));
    }catch(const std::exception& error){
      if(waiting)try{self->clearWaitingBat(*waiting);}catch(...){}
      if(self->inputs_.logError)try{self->inputs_.logError(error);}catch(...){}
    }catch(...){}
    self->finishBackgroundJob(keepAlive);
  }).detach();}catch(...){
    finishBackgroundJob(keepAlive);
    if(waiting){(void)forgetWaitingBat(*waiting);try{clearWaitingBat(*waiting);}catch(...){}}
    {std::lock_guard lock(capacityMutex_);if(outstanding_)--outstanding_;}
    notifyShutdownProgress();
    throw;
  }
}
void IndigenousBatApplication::runSkillBat(const std::string& description,
    bats::RunBatParams params){
  std::optional<std::string> registered;
  bool launched=false;
  try{
    auto services=makeServices();
    const auto id=services.launch.createBatId();
    rememberBatNode(id,std::nullopt,1);
    const auto preflight=makeRunPreflight();
    const auto& options=field(params.gadgetUseContext.query.data,"options");
    const auto model=preflight.getBatModel(field(params.batDefinition,"model"),
      field(options,"mainLoopModel"),params.model,
      field(field(appState(),"toolPermissionContext"),"mode"));
    bats::BatRegistrationParams registration;
    registration.batId=id;
    registration.description=description;
    const auto& first=params.poetryMessages.is_array()&&!params.poetryMessages.empty()?
      field(field(params.poetryMessages[0],"message"),"content"):Json();
    registration.poetry=string(first);
    registration.selectedBat=params.batDefinition;
    registration.model=model;
    const auto mission=bats::registerAsyncBat(registration,registration_);
    registered=id;
    bats::AsyncLifecycleParams lifecycle;
    lifecycle.missionId=id;
    lifecycle.abortOwner=mission.abortOwner;
    lifecycle.description=description;
    lifecycle.batIdForCleanup=id;
    lifecycle.gadgetUseContext=params.gadgetUseContext;
    lifecycle.rootSetAppState=missionState_.setAppState;
    lifecycle.metadata.poetry=registration.poetry;
    lifecycle.metadata.resolvedBatModel=model;
    lifecycle.metadata.batType=string(field(params.batDefinition,"agentType"));
    lifecycle.metadata.isBuiltInBat=field(params.batDefinition,"source")=="built-in";
    lifecycle.metadata.isAsync=true;
    lifecycle.metadata.startTime=platform::nowMillis();
    lifecycle.getWorktreeResult=[]{return Json::object();};
    params.isAsync=true;
    params.override["agentId"]=id;
    params.overrideAbortOwner=mission.abortOwner;
    lifecycle.makeStream=[run=services.async.runBat,params=std::move(params)](
        const std::function<void(const std::shared_ptr<bats::StashSafeParams>&)>& onStash,
        const query::Yield& yield){
      auto current=params;
      current.onStashSafeParams=onStash;
      run(current,yield);
    };
    launchLifecycle(std::move(lifecycle));
    launched=true;
  }catch(...){
    const auto error=std::current_exception();
    if(registered){
      std::string message="Skill Bat failed to start";
      try{std::rethrow_exception(error);}catch(const std::exception& failure){message=failure.what();}catch(...){}
      try{bats::failBatMission(*registered,message,missionState_);}catch(...){}
    }
    if(!launched)releaseCapacity();
    std::rethrow_exception(error);
  }
}
}
