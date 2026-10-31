#include "commands/fork/Internal.hpp"
#include <limits>

namespace batbox::commands::fork::internal {
namespace {
constexpr double kPollMs=200;
constexpr std::int64_t kSettleMs=10000;
struct BackgroundState {
  Prepared prepared;
  Services services;
  std::vector<query::MessageRef> messages;
  std::optional<Json> workload;
  std::int64_t deadline=0;
  event_loop::Loop::TimerHandle timer;
  BackgroundState(Prepared prepared,Services services):
    prepared(std::move(prepared)),services(std::move(services)){}
};
void enqueue(const std::shared_ptr<BackgroundState>& state,
  const std::string& text,bool failed){
  require(static_cast<bool>(state->services.enqueuePendingNotification),
    "enqueuePendingNotification");
  std::string value="<scheduled-task-result command=\"/"+
    state->prepared.commandName+"\"";
  if(failed)value+=" status=\"failed\"";
  value+=">\n"+text+"\n</scheduled-task-result>";
  Json queued={{"value",value},{"mode","prompt"},{"priority","later"},
    {"isMeta",true},{"skipSlashCommands",true}};
  if(state->workload)queued["workload"]=*state->workload;
  state->services.enqueuePendingNotification(queued);
}
void fail(const std::shared_ptr<BackgroundState>& state,
  const std::exception_ptr& error){
  const std::string message=errorMessage(error);
  if(state->services.logError)state->services.logError(message);
  enqueue(state,message,true);
}
bool hasPendingMcp(const Json& appState){
  const auto& clients=value::property(value::property(appState,"mcp"),"clients");
  if(!clients.is_array())return false;
  for(const auto& client:clients)if(value::property(client,"type")=="pending")return true;
  return false;
}
void poll(const std::shared_ptr<BackgroundState>& state){
  try{
    auto& services=state->services;
    require(static_cast<bool>(services.nowMs),"nowMs");
    require(static_cast<bool>(services.context.getAppState),"getAppState");
    require(static_cast<bool>(services.runBat),"runAgent");
    if(services.nowMs()<state->deadline&&hasPendingMcp(
      services.context.getAppState())){
      state->timer=services.loop.setTimeout(kPollMs,[state]{poll(state);});
      return;
    }
    const auto available=services.refreshGadgets?
      services.refreshGadgets():services.context.query.gadgets;
    auto params=paramsFor(state->prepared,services,true,available);
    auto pending=services.runBat(std::move(params),
      [state](const query::MessageRef& message){
      state->messages.push_back(message);
    });
    pending.then([state](const event_loop::Unit*,const std::exception_ptr& error){
      try{
        if(error){fail(state,error);return;}
        Json batMessages=Json::array();
        for(const auto& message:state->messages)batMessages.push_back(*message);
        const auto text=bats::extractResultText(batMessages,"Command completed");
        if(state->services.logDebug)state->services.logDebug(
          "Background forked command /"+state->prepared.commandName+
          " completed (agent "+state->prepared.batId+")");
        enqueue(state,text,false);
      }catch(...){fail(state,std::current_exception());}
    });
  }catch(...){fail(state,std::current_exception());}
}
} // namespace
event_loop::Deferred<user_input::InputResult> runBackground(
  Prepared prepared,Services& services){
  event_loop::Deferred<user_input::InputResult> output(services.loop);
  auto state=std::make_shared<BackgroundState>(std::move(prepared),services);
  try{
    require(static_cast<bool>(state->services.getWorkload),"getWorkload");
    require(static_cast<bool>(state->services.nowMs),"nowMs");
    state->workload=state->services.getWorkload();
    state->deadline=state->services.nowMs()+kSettleMs;
    poll(state);
    output.resolve(user_input::InputResult{});
  }catch(...){output.reject(std::current_exception());}
  return output;
}
} // namespace batbox::commands::fork::internal
