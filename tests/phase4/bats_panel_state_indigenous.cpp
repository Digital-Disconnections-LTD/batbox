#include "bats/BatGadgetForegroundProgress.hpp"
#include "bats/BatMissionForeground.hpp"
#include "bats/BatMissionRegistration.hpp"
#include "bats/BatMissionState.hpp"
#include "application_bats/StreamPublishThrottle.hpp"
#include <iostream>
#include <stdexcept>

using batbox::Json;
namespace bats=batbox::bats;
namespace value=batbox::value;
namespace {
void check(bool condition,const std::string& message){
  if(!condition)throw std::logic_error(message);
}
const Json& field(const Json& object,const char* key){return value::property(object,key);}
Json assistant(const Json& content,const Json& usage=Json::object()){
  return Json{{"type","assistant"},{"uuid","a"},
    {"message",{{"role","assistant"},{"content",content},{"usage",usage}}}};
}
Json textBlock(const std::string& text){return Json{{"type","text"},{"text",text}};}
Json gadgetBlock(const std::string& name){
  return Json{{"type","tool_use"},{"id","g-"+name},{"name",name},{"input",Json::object()}};
}

void lastTextLines(){
  check(bats::lastAssistantTextLine(assistant(Json::array({textBlock("first\n\n  Last line  \n\n")})))==
    "Last line","Last non-empty text line must be trimmed");
  check(bats::lastAssistantTextLine(assistant(Json::array({textBlock("early"),
    gadgetBlock("Read"),textBlock("later")})))=="later","The final text block wins");
  check(bats::lastAssistantTextLine(assistant(Json::array({gadgetBlock("Read")}))).empty(),
    "Gadget-only messages carry no text");
  check(bats::lastAssistantTextLine(Json{{"type","user"},{"message",{{"content","hi"}}}}).empty(),
    "Only assistant text counts");
  const auto clipped=bats::lastAssistantTextLine(assistant(Json::array({
    textBlock(std::string(200,'x'))})),20);
  check(value::stringLength(clipped)==20&&clipped.ends_with("…"),
    "Long lines are clipped to the requested characters");
  Json tracker=bats::createProgressTracker();
  bats::BatMissionProgressImports imports;
  imports.getGadgetSearchOrReadInfo=[](const std::string&,const Json&,
    const batbox::gadgets::Gadgets&){return Json::object();};
  bats::updateProgressFromMessage(tracker,assistant(Json::array({textBlock("Scanning files"),
    gadgetBlock("Grep")}),Json{{"input_tokens",100},{"output_tokens",20}}),{},{},imports);
  bats::updateProgressFromMessage(tracker,assistant(Json::array({gadgetBlock("Read")})),{},{},imports);
  const auto progress=bats::getProgressUpdate(tracker);
  check(progress.at("lastText")=="Scanning files"&&progress.at("toolUseCount")==2,
    "Progress keeps the latest text across Gadget-only turns");
  check(!bats::getProgressUpdate(bats::createProgressTracker()).contains("lastText"),
    "A Bat that has not spoken has no lastText");
}

void foregroundProgress(){
  std::vector<Json> published;
  bats::BatGadgetForegroundProgressServices services;
  services.normalizeMessages=[](const Json& value){return value;};
  bats::BatMissionProgressImports imports;
  imports.getGadgetSearchOrReadInfo=[](const std::string&,const Json&,
    const batbox::gadgets::Gadgets&){return Json::object();};
  services.updateProgressFromMessage=[&](Json& tracker,const Json& message,
      const bats::ActivityResolver& resolve,const batbox::gadgets::Gadgets& gadgets){
    bats::updateProgressFromMessage(tracker,message,resolve,gadgets,imports);
  };
  services.emitMissionProgress=[](const Json&,const std::string&,const Json&,
    const std::string&,std::int64_t,const std::string&){};
  services.getSdkBatProgressSummariesEnabled=[]{return false;};
  services.getProgressUpdate=bats::getProgressUpdate;
  services.updateAsyncBatProgress=[&](const std::string& id,const Json& progress,
      const bats::RootSetAppState&){
    check(id=="fg-1","Progress must target the foreground mission");
    published.push_back(progress);
  };
  services.getAssistantMessageContentLength=[](const Json&){return 0.0;};
  bats::BatContext context;
  context.updateResponseLength=[](const std::function<Json(const Json&)>&){};
  bats::BatMessageRefs refs;Json tracker=bats::createProgressTracker();
  const std::optional<std::string> mission("fg-1");
  const auto feed=[&](const Json& message){
    bats::processBatGadgetForegroundMessage(std::make_shared<Json>(message),refs,tracker,{},{},
      mission,Json("gadget-use"),"Foreground scan",1,"poetry","fg-1",
      Json{{"message",{{"id","leader"}}}},[](const auto&){},context,services);
  };
  feed(assistant(Json::array({textBlock("Looking around"),gadgetBlock("Glob")}),
    Json{{"input_tokens",50},{"output_tokens",5}}));
  check(published.size()==1&&published[0].at("toolUseCount")==1&&
    published[0].at("tokenCount")==55&&published[0].at("lastText")=="Looking around",
    "Foreground Bats must publish progress without SDK summaries");
  feed(Json{{"type","user"},{"uuid","u"},{"message",{{"role","user"},{"content",
    Json::array({Json{{"type","tool_result"},{"tool_use_id","g-Glob"},{"content","ok"}}})}}}});
  check(published.size()==1,"Gadget results do not change progress and are not republished");
  feed(assistant(Json::array({textBlock("Found it")})));
  check(published.size()==2&&published[1].at("lastText")=="Found it",
    "Every assistant turn republishes the counters");
}

struct StateFixture {
  Json state=Json{{"tasks",Json::object()}};
  bats::BatMissionStateServices missionState;
  bats::BatRegistrationServices registration;
  bats::ForegroundSignals signals;
  bats::ForegroundServices foreground;
  StateFixture(){
    missionState.getAppState=[this]{return state;};
    missionState.setAppState=[this](const auto& update){state=update(state);};
    missionState.nowMillis=[]{return std::int64_t{1000};};
    missionState.unregisterCleanup=[](const std::string&){};
    missionState.abortMission=[](const std::string&){};
    missionState.evictMissionOutput=[](const std::string&){};
    registration.state=&missionState;
    registration.getMissionOutputPath=[](const std::string& id){return "/out/"+id;};
    registration.getBatTranscriptPath=[](const std::string& id){return "/t/"+id;};
    registration.initMissionOutputAsSymlink=[](const std::string&,const std::string&){};
    registration.registerCleanup=[](const std::function<void()>&){return std::function<void()>([]{});};
    registration.enqueueSdkEvent=[](const Json&){};
    foreground.registration=&registration;foreground.signals=&signals;
  }
};
void registrationIdentity(){
  StateFixture fixture;
  bats::BatRegistrationParams params;
  params.batId="bg-1";params.description="Background review";params.poetry="Review";
  params.name="reviewer";params.model="fixture-small";
  params.selectedBat={{"agentType","general-purpose"}};
  (void)bats::registerAsyncBat(params,fixture.registration);
  const auto& mission=fixture.state.at("tasks").at("bg-1");
  check(mission.at("name")=="reviewer"&&mission.at("model")=="fixture-small",
    "Background registration must record the Bat's name and model");
  fixture.state["tasks"]["bg-1"]["retain"]=true;
  fixture.state["tasks"]["bg-1"]["status"]="completed";
  bats::BatRegistrationParams resumed=params;resumed.name.clear();
  (void)bats::registerAsyncBat(resumed,fixture.registration);
  check(fixture.state.at("tasks").at("bg-1").at("name")=="reviewer",
    "A resumed Bat keeps the name it was launched with");
  bats::BatRegistrationParams unnamed;unnamed.batId="bg-2";unnamed.description="d";
  unnamed.selectedBat=Json::object();
  (void)bats::registerAsyncBat(unnamed,fixture.registration);
  check(!fixture.state.at("tasks").at("bg-2").contains("name")&&
    !fixture.state.at("tasks").at("bg-2").contains("model"),
    "Unnamed Bats do not get empty name or model fields");
  bats::BatRegistrationParams foreground=params;foreground.batId="fg-1";
  (void)bats::registerBatForeground(foreground,std::nullopt,fixture.foreground);
  const auto& live=fixture.state.at("tasks").at("fg-1");
  check(live.at("name")=="reviewer"&&live.at("model")=="fixture-small"&&
    live.at("isBackgrounded")==false,"Foreground registration must record identity too");
}

Json settled(const std::string& status,Json evict,std::int64_t end,bool retain=false){
  return Json{{"type","local_agent"},{"agentType","general-purpose"},{"status",status},
    {"agentId","x"},{"description","settled "+status},{"startTime",end-100},{"endTime",end},
    {"evictAfter",std::move(evict)},{"retain",retain},{"prompt",std::string(5000,'p')},
    {"selectedAgent",Json::object()},
    {"progress",{{"toolUseCount",4},{"tokenCount",99},{"lastText","done"},
      {"recentActivities",Json::array({Json{{"toolName","Read"},{"input",{{"file","big"}}}}})},
      {"lastActivity",{{"toolName","Read"},{"input",{{"file","big"}}}}}}},
    {"result",{{"totalTokens",99},{"totalToolUseCount",4},{"totalDurationMs",100},
      {"content",Json::array({textBlock(std::string(4000,'r'))})},{"usage",Json::object()}}}};
}
void settledSweep(){
  Json tasks={{"running",{{"type","local_agent"},{"status","running"},{"agentType","general-purpose"}}},
    {"fresh",settled("completed",5000,900)},
    {"expired",settled("failed",500,800)},
    {"dismissed",settled("completed",0,700)},
    {"observed",settled("completed",500,600,true)},
    {"busy",settled("killed",500,650)},
    {"shell",{{"type","local_bash"},{"status","completed"},{"evictAfter",0}}}};
  auto sweep=bats::sweepSettledBatMissions(tasks,1000,50,
    [](const std::string& id){return id=="busy";});
  check(sweep.missions.has_value(),"Expired and dismissed records must change the tasks");
  const auto& next=*sweep.missions;
  check(!next.contains("dismissed")&&sweep.removed==1,"Dismissed Bats are removed");
  check(next.contains("fresh")&&next.at("fresh").at("prompt").get<std::string>().size()==5000,
    "Records inside their grace window are untouched");
  check(sweep.nextDueMs==5000,"The next sweep is due at the earliest pending evictAfter");
  const auto& reduced=next.at("expired");
  check(reduced.at("summarized")==true&&!reduced.contains("selectedAgent")&&
    !field(reduced,"result").contains("content")&&!field(reduced,"result").contains("usage")&&
    !field(reduced,"progress").contains("recentActivities")&&
    !field(field(reduced,"progress"),"lastActivity").contains("input")&&
    value::stringLength(reduced.at("prompt").get<std::string>())==2000&&
    reduced.at("progress").at("lastText")=="done"&&reduced.at("result").at("totalTokens")==99&&
    reduced.at("status")=="failed"&&reduced.at("agentId")=="x"&&
    reduced.at("description")=="settled failed",
    "Expired records keep only the fields the Bats list and leader need");
  check(next.at("observed")==tasks.at("observed")&&next.at("busy")==tasks.at("busy")&&
    next.at("running")==tasks.at("running")&&next.at("shell")==tasks.at("shell"),
    "Observed, busy, running and non-Bat records are never swept");
  const auto again=bats::sweepSettledBatMissions(next,1000,50,
    [](const std::string& id){return id=="busy";});
  check(!again.missions,"Sweeping an already-swept set changes nothing");

  Json many=Json::object();
  for(int i=0;i<55;++i)many["bat-"+std::to_string(i)]=settled("completed",10,100+i);
  const auto capped=bats::sweepSettledBatMissions(many,1000,50);
  check(capped.missions&&capped.missions->size()==50&&capped.removed==5&&
    !capped.missions->contains("bat-0")&&capped.missions->contains("bat-54"),
    "Only the newest settled Bat records are kept");
}

void savedRecord(){
  const auto record=bats::savedBatRecord("a1b2c3d4","Saved review","Explore",
    std::string(3000,'q'),100,200,6,assistant(Json::array({textBlock("All checks pass")}),
      Json{{"input_tokens",10},{"cache_read_input_tokens",5},{"output_tokens",2}}),"/out/a1b2c3d4");
  check(record.at("status")=="completed"&&record.at("type")=="local_agent"&&
    record.at("diskLoaded")==false&&record.at("retain")==false&&
    record.at("notified")==true&&record.at("retrieved")==true&&
    !record.contains("evictAfter")&&record.at("agentId")=="a1b2c3d4"&&
    record.at("progress").at("lastText")=="All checks pass"&&
    record.at("progress").at("toolUseCount")==6&&record.at("progress").at("tokenCount")==17&&
    value::stringLength(record.at("prompt").get<std::string>())==2000,
    "Saved Bats are settled, unloaded, already notified records");
  check(bats::isPanelBatMission(record),"Saved Bats are listable panel Bats");
}

void throttle(){
  batbox::application_bats::StreamPublishThrottle gate(100);
  using Decision=batbox::application_bats::StreamPublishThrottle::Decision;
  auto step=gate.onEvent("b",1000,false);
  check(step.decision==Decision::Now,"The first delta publishes at once");
  gate.published("b",1000);
  step=gate.onEvent("b",1030,false);
  check(step.decision==Decision::Schedule&&step.delayMs==70,
    "A delta inside the interval schedules one trailing publish");
  for(int i=0;i<40;++i)
    check(gate.onEvent("b",1031+i,false).decision==Decision::Pending,
      "Further deltas wait for the scheduled publish");
  check(gate.onEvent("b",1050,true).decision==Decision::Now,
    "Complete messages always publish at once");
  gate.published("b",1050);
  gate.timerFired("b");gate.published("b",1100);
  check(gate.onEvent("b",1150,false).decision==Decision::Schedule,
    "After the trailing publish the interval restarts");
  check(gate.onEvent("other",1150,false).decision==Decision::Now,
    "Each Bat is throttled independently");
}
}
int main(){
  try{
    lastTextLines();foregroundProgress();registrationIdentity();settledSweep();
    savedRecord();throttle();
    std::cout<<"Bat panel state: last text, foreground progress, identity, sweep and throttle hold\n";
    return 0;
  }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
