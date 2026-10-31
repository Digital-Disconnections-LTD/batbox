#include "sdk/IndigenousBindings.hpp"
#include <cassert>
#include <iostream>
using namespace batbox;
int main(){
 bool active=true;std::string flight="native-session",enabled="true";
 auto queue=std::make_shared<sdk::EventQueue>(sdk::indigenousQueueServices([&]{return active;},[&]{return flight;}));
 flights::Lifecycle state(sdk::lifecycleQueueServices(queue,[&](const std::string& key)->std::optional<std::string>{
  assert(key=="BATBOX_EMIT_SESSION_STATE_EVENTS");return enabled;
 }));
 state.notifyFlightStateChanged("running");
 auto first=queue->drainSdkEvents();assert(first.size()==1);
 assert(first[0]->snapshot().at("session_id")==flight);
 assert(first[0]->snapshot().at("uuid").get<std::string>().size()==36);
 enabled="false";state.notifyFlightStateChanged("idle");assert(queue->drainSdkEvents().empty());
 Json app={{"tasks",Json::object()}};bats::BatMissionStateServices missionState;
 missionState.setAppState=[&](const std::function<Json(const Json&)>& fn){app=fn(app);};
 bats::BatRegistrationServices bat;bat.state=&missionState;sdk::bindBatSdkQueue(bat,queue);
 bats::registerMission(Json{{"id","worker"},{"type","local_agent"},{"description","Work"}},bat);
 const auto started=queue->drainSdkEvents();assert(started.size()==1);
 assert(started[0]->snapshot().at("subtype")=="task_started");assert(app["tasks"].contains("worker"));
 bats::BatGadgetForegroundFinalServices foreground;sdk::bindForegroundSdkQueue(foreground,queue);
 foreground.enqueueSdkEvent(Json{{"type","system"},{"subtype","task_notification"},{"status","completed"}});
 const auto done=queue->drainSdkEvents();assert(done.size()==1&&done[0]->snapshot().at("status")=="completed");
 active=false;foreground.enqueueSdkEvent(Json{{"type","system"}});assert(queue->drainSdkEvents().empty());
 std::cout<<"Native lifecycle environment, UUID, agent registration and foreground SDK queue bindings pass\n";
}
