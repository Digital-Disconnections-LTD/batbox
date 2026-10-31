#include "flights/Activity.hpp"
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <thread>

using namespace batbox;
int main(){
  event_loop::Loop loop;cleanup::Roster roster(loop);
  Json events=Json::array();
  auto services=flights::indigenousActivityServices(loop,roster,
    [&](const std::string&,const std::string& event,Json){
      assert(loop.isOwnerThread());events.push_back(event);});
  const auto old=std::getenv("BATBOX_REMOTE_SEND_KEEPALIVES");
  const std::optional<std::string> previous=old?std::optional<std::string>(old):std::nullopt;
  ::setenv("BATBOX_REMOTE_SEND_KEEPALIVES","yes",1);
  // Only the test scales elapsed time; production uses 30000 ms.
  auto interval=services.setInterval,timeout=services.setTimeout;
  services.setInterval=[interval](double ms,std::function<void()> callback){
    assert(ms==30000);return interval(ms/10000,std::move(callback));};
  services.setTimeout=[timeout](double ms,std::function<void()> callback){
    assert(ms==30000);return timeout(ms/10000,std::move(callback));};
  flights::Activity activity(services);int callbacks=0;
  activity.registerFlightActivityCallback([&]{
    assert(loop.isOwnerThread());callbacks++;
    activity.stopFlightActivity("api_call");
  });
  activity.startFlightActivity("api_call");
  assert(roster.size()==1);
  loop.run();
  assert(callbacks==1);
  assert(events==Json::array({"session_keepalive_heartbeat","session_idle_30s"}));
  bool cleaned=false;
  roster.runCleanupFunctions().then([&](const event_loop::Unit*,const std::exception_ptr& e){
    if(e)std::rethrow_exception(e);cleaned=true;});
  loop.run();assert(cleaned);
  assert(events.back()=="session_activity_at_shutdown");
  activity.unregisterFlightActivityCallback();
  assert(!activity.isFlightActivityTrackingActive());
  if(previous)::setenv("BATBOX_REMOTE_SEND_KEEPALIVES",previous->c_str(),1);
  else ::unsetenv("BATBOX_REMOTE_SEND_KEEPALIVES");
}
