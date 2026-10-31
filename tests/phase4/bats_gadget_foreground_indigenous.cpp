#include "bats/BatGadgetForeground.hpp"
#include "runtime/MessageSnapshot.hpp"
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
using namespace std::chrono_literals;
using batbox::Json;
namespace {
const Json& field(const Json& value,const char* key){return batbox::value::property(value,key);}
struct Fixture {
  Json state={{"tasks",Json::object()}};
  Json sdkEvents=Json::array();
  batbox::bats::BatMissionStateServices missionState;
  batbox::bats::BatRegistrationServices registration;
  batbox::bats::ForegroundSignals signals;
  batbox::bats::ForegroundServices foreground;
  batbox::bats::BatGadgetForegroundServices services;
  batbox::bats::BatContext context;
  batbox::bats::BatGadgetLaunchPlan plan;
  std::int64_t time=1000;
  double responseLength=0;
  bool background=false,launched=false;
  Fixture(){
    batbox::bats::RootSetAppState set=[this](const std::function<Json(const Json&)>& update){
      state=update(state);
    };
    missionState.setAppState=set;
    missionState.getAppState=[this](){return state;};
    missionState.nowMillis=[this](){return time;};
    missionState.unregisterCleanup=[](const std::string&){};
    registration.state=&missionState;
    registration.getMissionOutputPath=[](const std::string& id){return "/out/"+id;};
    registration.getBatTranscriptPath=[](const std::string& id){return "/transcript/"+id;};
    registration.initMissionOutputAsSymlink=[](const std::string&,const std::string&){};
    registration.registerCleanup=[](const std::function<void()>&){return [](){};};
    registration.enqueueSdkEvent=[this](const Json& event){sdkEvents.push_back(event);};
    foreground.registration=&registration;
    foreground.signals=&signals;
    context.query.data={{"toolUseId","tool-1"}};
    context.abortOwner=std::make_shared<std::stop_source>();
    context.getAppState=[this](){return state;};
    context.setAppState=set;
    context.setAppStateForMissions=set;
    context.updateResponseLength=[this](const std::function<Json(const Json&)>& update){
      responseLength=update(responseLength).get<double>();
    };
    context.setGadgetJSX=[](const Json&){};
    plan.earlyBatId="agent-1";
    plan.rootSetAppState=set;
    plan.runBatParams.batDefinition={{"agentType","Explore"}};
    plan.runBatParams.poetryMessages=Json::array({Json{{"type","user"},
      {"message",{{"content",Json::array()}}}}});
    plan.metadata={{"prompt","Do work"},{"resolvedAgentModel","model"},
      {"isBuiltInAgent",true},{"startTime",time},
      {"agentType","Explore"},{"isAsync",false}};
    services.foreground=&foreground;
    services.nowMillis=[this](){return time;};
    services.isBackgroundMissionsDisabled=[](){return false;};
    services.getSdkBatProgressSummariesEnabled=[](){return false;};
    services.getAutoBackgroundMs=[](){return 0.;};
    services.getParentFlightId=[]()->std::optional<std::string>{return std::nullopt;};
    services.isBuiltInBat=[](const Json&){return true;};
    services.runWithBatContext=[](const Json&,const std::function<void()>& fn){fn();};
    services.runWithCwdOverride=[](const std::string&,const std::function<void()>& fn){fn();};
    services.renderBackgroundHint=[](){return Json{{"component","BackgroundHint"}};};
    services.getMissionOutputPath=[](const std::string& id){return "/out/"+id;};
    services.progress.normalizeMessages=[](const Json& messages){return messages;};
    services.progress.updateProgressFromMessage=[](Json& tracker,const Json& message,
        const batbox::bats::ActivityResolver&,const batbox::gadgets::Gadgets&){
      if(field(message,"type")=="assistant")
        tracker["toolUseCount"]=batbox::bats::countGadgetUses(Json::array({message}));
    };
    services.progress.onProgress=[this](const Json& value){
      if(background && field(field(value,"data"),"prompt")=="" &&
          !launched){
        launched=batbox::bats::backgroundBatMission("agent-1",foreground);
      }
    };
    services.progress.emitMissionProgress=[](const Json&,const std::string&,
      const Json&,const std::string&,std::int64_t,const std::string&){};
    services.progress.getSdkBatProgressSummariesEnabled=[](){return false;};
    services.progress.getProgressUpdate=[](const Json& tracker){return tracker;};
    services.progress.updateAsyncBatProgress=[](const std::string&,const Json&,
      const batbox::bats::RootSetAppState&){};
    services.progress.getAssistantMessageContentLength=[](const Json&){return 2.;};
    services.final.getProgressUpdate=[](const Json& tracker){
      return Json{{"tokenCount",0},{"toolUseCount",field(tracker,"toolUseCount")}};
    };
    services.final.enqueueSdkEvent=[this](const Json& event){sdkEvents.push_back(event);};
    services.final.clearInvokedSkillsForBat=[](const std::string&){};
    services.final.nowMillis=[this](){return time;};
    services.final.isSyntheticMessage=[](const Json&){return false;};
    services.final.feature=[](const std::string&){return false;};
    services.final.result.getLastAssistantMessage=[](const Json& messages){
      for(auto it=messages.rbegin();it!=messages.rend();++it)
        if(field(*it,"type")=="assistant")return *it;
      return Json();
    };
    services.final.result.getTokenCountFromUsage=[](const Json& usage){
      return batbox::value::numberOr(field(usage,"input_tokens"))+
        batbox::value::numberOr(field(usage,"output_tokens"));
    };
    services.final.result.nowMillis=[this](){return time;};
    services.launchBackgroundContinuation=[this](batbox::bats::BatGadgetBackgroundContinuation next){
      launched=true;
      assert(next.missionId=="agent-1");
      assert(next.foregroundStream);
      assert(next.missionAbortOwner);
      next.foregroundStream->returnWithin(1000ms);
    };
  }
  Json run(){
    return batbox::bats::runBatGadgetForeground(
      Json{{"prompt","Do work"},{"description","Explore"}},
      Json{{"message",{{"id","m1"}}}},context,plan,services);
  }
};
}
int main(){
  {
    Fixture fixture;
    auto ref=std::make_shared<Json>(Json{{"type","assistant"},
      {"message",{{"content",Json::array({Json{{"type","text"},{"text","ok"}}})},
        {"usage",{{"input_tokens",1},{"output_tokens",1}}}}}});
    fixture.services.runBat=[ref](const batbox::bats::RunBatParams&,
        const batbox::query::Yield& yield){
      yield(ref);
      batbox::runtime::MessageLock lock(ref);
      lock.value()["message"]["usage"]["output_tokens"]=42;
    };
    const Json result=fixture.run();
    assert(result.at("data").at("status")=="completed");
    assert(result.at("data").at("usage").at("output_tokens")==42);
    assert(fixture.responseLength==2);
    assert(fixture.sdkEvents.back().at("status")=="completed");
  }
  {
    Fixture fixture;
    fixture.background=true;
    fixture.services.runBat=[](const batbox::bats::RunBatParams& request,
        const batbox::query::Yield& yield){
      yield(Json{{"type","assistant"},
        {"message",{{"content",Json::array({Json{{"type","tool_use"},
          {"name","Read"},{"input",Json::object()}}})},
          {"usage",{{"input_tokens",1},{"output_tokens",1}}}}}});
      std::this_thread::sleep_for(30ms);
      if(request.overrideAbortOwner->stop_requested())return;
      yield(Json{{"type","assistant"},
        {"message",{{"content",Json::array()},{"usage",Json::object()}}}});
    };
    const Json result=fixture.run();
    assert(result.at("data").at("status")=="async_launched");
    assert(fixture.launched);
    assert(fixture.state.at("tasks").at("agent-1").at("isBackgrounded")==true);
  }
  {
    Fixture fixture;
    Json jsxUpdates=Json::array();
    fixture.context.setGadgetJSX=[&](const Json& value){jsxUpdates.push_back(value);};
    int reads=0;
    fixture.services.nowMillis=[&](){return reads++==0?1000:3001;};
    fixture.services.runBat=[](const batbox::bats::RunBatParams&,
        const batbox::query::Yield& yield){
      yield(Json{{"type","assistant"},{"message",{{"content",Json::array({
        Json{{"type","text"},{"text","ok"}}})},
        {"usage",{{"input_tokens",1},{"output_tokens",1}}}}}});
    };
    fixture.run();
    assert(jsxUpdates.size()==2);
    assert(jsxUpdates[0].at("jsx").at("component")=="BackgroundHint");
    assert(jsxUpdates[0].at("shouldHidePromptInput")==false);
    assert(jsxUpdates[0].at("shouldContinueAnimation")==true);
    assert(jsxUpdates[0].at("showSpinner")==true);
    assert(jsxUpdates[1].is_null());
  }
  std::cout<<"BatGadget foreground completion, late mutation and background race passed\n";
  return 0;
}
