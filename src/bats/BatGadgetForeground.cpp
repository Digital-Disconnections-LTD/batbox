#include "bats/BatGadgetForeground.hpp"
#include <mutex>
#include <stdexcept>

namespace batbox::bats {
namespace {
const Json& field(const Json& value,const char* key){return value::property(value,key);}
std::string text(const Json& value){return value.is_string()?value.get<std::string>():"";}
template<class F>void need(const F& fn,const char* name){
  if(!fn)throw std::invalid_argument(std::string("BatGadget foreground requires ")+name);
}
struct SummaryStop {
  std::mutex mutex;
  std::function<void()> stop;
  void set(std::function<void()> fn){std::lock_guard lock(mutex);stop=std::move(fn);}
  void invoke(){
    std::function<void()> fn;
    {std::lock_guard lock(mutex);fn=std::move(stop);}
    if(fn)fn();
  }
};
BatResultMetadata metadataFrom(const Json& value){
  BatResultMetadata out;
  out.poetry=text(field(value,"prompt"));
  out.resolvedBatModel=text(field(value,"resolvedAgentModel"));
  out.batType=text(field(value,"agentType"));
  out.isBuiltInBat=field(value,"isBuiltInAgent")==true;
  out.isAsync=field(value,"isAsync")==true;
  if(field(value,"startTime").is_number_integer())
    out.startTime=field(value,"startTime").get<std::int64_t>();
  return out;
}
bool canReadOutput(const gadgets::Gadgets& available,const std::string& read,
    const std::string& shell){
  for(const auto* gadget:available)
    if(gadget && (gadgets::gadgetMatchesName(*gadget,read) ||
                gadgets::gadgetMatchesName(*gadget,shell)))return true;
  return false;
}
} // namespace
Json runBatGadgetForeground(const Json& input,const Json& assistantMessage,
    const BatContext& context,BatGadgetLaunchPlan plan,
    BatGadgetForegroundServices& services){
  if(plan.shouldRunAsync)throw std::invalid_argument("BatGadget foreground requires a sync plan");
  need(services.runBat,"runAgent");
  need(services.nowMillis,"nowMillis");
  need(services.isBackgroundMissionsDisabled,"isBackgroundTasksDisabled");
  need(services.getSdkBatProgressSummariesEnabled,
    "getSdkAgentProgressSummariesEnabled");
  need(services.getParentFlightId,"getParentSessionId");
  need(services.isBuiltInBat,"isBuiltInAgent");
  need(services.runWithBatContext,"runWithAgentContext");
  need(services.getMissionOutputPath,"getTaskOutputPath");
  const std::string syncBatId=plan.earlyBatId;
  const std::string description=text(field(input,"description"));
  const std::string poetry=text(field(input,"prompt"));
  Json batContext={{"agentId",syncBatId},{"agentType","subagent"},
    {"subagentName",field(plan.runBatParams.batDefinition,"agentType")},
    {"isBuiltIn",services.isBuiltInBat(plan.runBatParams.batDefinition)},
    {"invocationKind","spawn"},{"invocationEmitted",false}};
  const auto parentFlight=services.getParentFlightId();
  if(parentFlight)batContext["parentSessionId"]=*parentFlight;
  if(assistantMessage.is_object() && assistantMessage.contains("requestId"))
    batContext["invokingRequestId"]=assistantMessage.at("requestId");
  Json answer;
  auto body=[&](){
    BatGadgetForegroundFinalState final;
    final.syncBatId=syncBatId;
    final.description=description;
    final.poetry=poetry;
    final.selectedBat=plan.runBatParams.batDefinition;
    final.metadata=metadataFrom(plan.metadata);
    final.context=context;
    final.rootSetAppState=plan.rootSetAppState;
    final.gadgetUseId=field(context.query.data,"toolUseId");
    final.batStartTime=services.nowMillis();
    final.syncTracker=createProgressTracker();
    auto resolveActivity=createActivityDescriptionResolver(context.query.gadgets);
    auto worktree=std::make_shared<BatGadgetWorktreeState>(plan.worktree);
    final.worktree=worktree;
    emitInitialBatGadgetProgress(plan.runBatParams.poetryMessages,poetry,
      syncBatId,assistantMessage,services.progress);
    std::optional<RegisteredForegroundBat> foreground;
    std::optional<ObservedBackgroundSignal> backgroundSignal;
    if(!services.isBackgroundMissionsDisabled()){
      if(!services.foreground)throw std::invalid_argument("BatGadget foreground roster is unbound");
      need(services.getAutoBackgroundMs,"getAutoBackgroundMs");
      BatRegistrationParams registration;
      registration.batId=syncBatId;
      registration.description=description;
      registration.poetry=poetry;
      registration.selectedBat=plan.runBatParams.batDefinition;
      registration.name=text(field(input,"name"));
      registration.model=plan.resolvedBatModel;
      const auto& gadgetUseId=field(context.query.data,"toolUseId");
      if(gadgetUseId.is_string())registration.gadgetUseId=gadgetUseId.get<std::string>();
      const double autoBackgroundMs=services.getAutoBackgroundMs();
      foreground=registerBatForeground(registration,
        autoBackgroundMs?std::optional<double>(autoBackgroundMs):std::nullopt,
        *services.foreground);
      final.foregroundMissionId=foreground->missionId;
      backgroundSignal=ObservedBackgroundSignal{
        foreground->backgroundSignal,foreground->backgroundCompletion};
      final.cancelAutoBackground=foreground->cancelAutoBackground;
    }
    auto summaryStop=std::make_shared<SummaryStop>();
    final.stopForegroundSummarization=[summaryStop](){summaryStop->invoke();};
    RunBatParams run=plan.runBatParams;
    run.override["agentId"]=syncBatId;
    if(final.foregroundMissionId && services.getSdkBatProgressSummariesEnabled()){
      need(services.startBatSummarization,"startAgentSummarization");
      const auto missionId=*final.foregroundMissionId;
      const auto startSummary=services.startBatSummarization;
      const auto rootSet=plan.rootSetAppState;
      run.onStashSafeParams=[summaryStop,missionId,syncBatId,startSummary,rootSet]
          (const std::shared_ptr<StashSafeParams>& params){
        summaryStop->set(startSummary(missionId,syncBatId,params,rootSet));
      };
    }
    const auto rawRunner=services.runBat;
    const auto runInContext=services.runWithBatContext;
    const auto runInCwd=services.runWithCwdOverride;
    const auto cwd=plan.cwdOverridePath;
    BatGadgetStream::Runner wrappedRunner=[rawRunner,runInContext,runInCwd,cwd,
        batContext](const RunBatParams& params,const query::Yield& yield){
      runInContext(batContext,[&](){
        auto invoke=[&](){rawRunner(params,yield);};
        if(cwd){need(runInCwd,"runWithCwdOverride");runInCwd(*cwd,invoke);}
        else invoke();
      });
    };
    const auto parentStop=context.abortOwner?
      context.abortOwner->get_token():context.query.stop;
    auto stream=std::make_shared<BatGadgetStream>(std::move(run),
      std::move(wrappedRunner),parentStop);
    bool backgroundHintShown=false;
    std::optional<Json> backgroundResult;
    try{
      while(true){
        const auto elapsed=services.nowMillis()-final.batStartTime;
        if(!services.isBackgroundMissionsDisabled() && !backgroundHintShown &&
            elapsed>=2000 && context.setGadgetJSX){
          backgroundHintShown=true;
          need(services.renderBackgroundHint,"renderBackgroundHint");
          context.setGadgetJSX(Json{{"jsx",services.renderBackgroundHint()},
            {"shouldHidePromptInput",false},
            {"shouldContinueAnimation",true},{"showSpinner",true}});
        }
        const auto nextMessage=stream->nextObserved();
        const auto race=raceBatMessageAndBackground(nextMessage,backgroundSignal);
        if(race.background && final.foregroundMissionId){
          need(context.getAppState,"toolUseContext.getAppState");
          const Json current=effectiveBatGetAppState(context)();
          const Json& mission=value::property(field(current,"tasks"),
            *final.foregroundMissionId);
          if(isLocalBatMission(mission) && value::truthy(field(mission,"isBackgrounded"))){
            final.wasBackgrounded=true;
            summaryStop->invoke();
            need(services.launchBackgroundContinuation,
              "launchBackgroundContinuation");
            BatGadgetBackgroundContinuation continuation;
            continuation.missionId=*final.foregroundMissionId;
            continuation.syncBatId=syncBatId;
            continuation.description=description;
            continuation.gadgetUseId=final.gadgetUseId;
            continuation.selectedBat=final.selectedBat;
            continuation.metadata=final.metadata;
            continuation.runBatParams=plan.runBatParams;
            continuation.context=context;
            continuation.rootSetAppState=plan.rootSetAppState;
            continuation.existingMessages=final.batMessages;
            continuation.foregroundStream=stream;
            continuation.missionAbortOwner=foreground->registered.abortOwner;
            continuation.worktree=worktree;
            services.runWithBatContext(batContext,[&](){
              services.launchBackgroundContinuation(std::move(continuation));
            });
            backgroundResult=Json{{"data",{{"isAsync",true},
              {"status","async_launched"},{"agentId",*final.foregroundMissionId},
              {"description",description},{"prompt",poetry},
              {"outputFile",services.getMissionOutputPath(*final.foregroundMissionId)},
              {"canReadOutputFile",canReadOutput(context.query.gadgets,
                services.fileReadGadgetName,services.bashGadgetName)}}}};
            break;
          }
        }
        if(race.background)continue;
        if(!race.message)break;
        processBatGadgetForegroundMessage(*race.message,final.batMessages,
          final.syncTracker,resolveActivity,context.query.gadgets,
          final.foregroundMissionId,final.gadgetUseId,description,final.batStartTime,
          poetry,syncBatId,assistantMessage,plan.rootSetAppState,
          context,services.progress);
      }
    }catch(const BatAbortError&){
      final.wasAborted=true;
      final.abortError=std::current_exception();
    }catch(...){
      final.syncBatError=std::current_exception();
      if(services.reportForegroundError)try{
        services.reportForegroundError(final.syncBatError);
      }catch(...){}
    }
    auto finalServices=services.final;
    finalServices.foreground=services.foreground;
    const auto finalized=finalizeBatGadgetForeground(final,finalServices);
    answer=backgroundResult?*backgroundResult:*finalized;
  };
  services.runWithBatContext(batContext,[&](){
    if(plan.cwdOverridePath){
      need(services.runWithCwdOverride,"runWithCwdOverride");
      services.runWithCwdOverride(*plan.cwdOverridePath,body);
    }else body();
  });
  return answer;
}
} // namespace batbox::bats
