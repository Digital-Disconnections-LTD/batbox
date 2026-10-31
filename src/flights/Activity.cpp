#include "flights/Activity.hpp"
#include <optional>
#include <stdexcept>

namespace batbox::flights {
namespace {constexpr double intervalMs=30000;}
struct Activity::State:std::enable_shared_from_this<State> {
  ActivityServices services;
  std::function<void()> callback;
  double refcount=0;
  Json reasons=Json::object();
  std::optional<double> oldestStartedAt;
  std::optional<cleanup::ShutdownTimer> heartbeat,idle;
  bool cleanupRegistered=false;
  explicit State(ActivityServices value):services(std::move(value)){}
  ~State(){if(heartbeat)heartbeat->cancel();if(idle)idle->cancel();}
  void clearIdle(){
    if(idle){idle->cancel();idle.reset();}
  }
  void startHeartbeat(){
    clearIdle();
    std::weak_ptr<State> weak=shared_from_this();
    heartbeat=services.setInterval(intervalMs,[weak]{
      if(auto self=weak.lock()){
        self->services.diagnostic("debug","session_keepalive_heartbeat",
          Json{{"refcount",self->refcount}});
        if(self->services.sendKeepalives()&&self->callback)self->callback();
      }
    });
  }
  void startIdle(){
    clearIdle();
    if(!callback)return;
    std::weak_ptr<State> weak=shared_from_this();
    idle=services.setTimeout(intervalMs,[weak]{
      if(auto self=weak.lock()){
        self->services.diagnostic("info","session_idle_30s",Json());
        self->idle.reset();
      }
    });
  }
};
Activity::Activity(ActivityServices services):state_(std::make_shared<State>(std::move(services))){
  const auto& s=state_->services;
  if(!s.now||!s.sendKeepalives||!s.setInterval||!s.setTimeout||!s.diagnostic||!s.registerCleanup)
    throw std::invalid_argument("Session activity imports required");
}
void Activity::registerFlightActivityCallback(std::function<void()> callback){
  auto& s=*state_;s.callback=std::move(callback);
  if(s.refcount>0&&!s.heartbeat)s.startHeartbeat();
}
void Activity::unregisterFlightActivityCallback(){
  auto& s=*state_;s.callback={};
  if(s.heartbeat){s.heartbeat->cancel();s.heartbeat.reset();}
  s.clearIdle();
}
void Activity::sendFlightActivitySignal(){
  auto& s=*state_;
  if(s.services.sendKeepalives()&&s.callback)s.callback();
}
bool Activity::isFlightActivityTrackingActive()const{return bool(state_->callback);}
void Activity::startFlightActivity(const std::string& reason){
  auto& s=*state_;s.refcount++;
  s.reasons[reason]=s.reasons.value(reason,0.0)+1;
  if(s.refcount==1){
    s.oldestStartedAt=s.services.now();
    if(s.callback&&!s.heartbeat)s.startHeartbeat();
  }
  if(!s.cleanupRegistered){
    s.cleanupRegistered=true;
    const auto state=state_;
    s.services.registerCleanup([state]{
      auto& s=*state;
      s.services.diagnostic("info","session_activity_at_shutdown",Json{
        {"refcount",s.refcount},{"active",s.reasons},
        {"oldest_activity_ms",s.refcount>0&&s.oldestStartedAt?
          Json(s.services.now()-*s.oldestStartedAt):Json()}});
    });
  }
}
void Activity::stopFlightActivity(const std::string& reason){
  auto& s=*state_;
  if(s.refcount>0)s.refcount--;
  const auto n=s.reasons.value(reason,0.0)-1;
  if(n>0)s.reasons[reason]=n;else s.reasons.erase(reason);
  if(s.refcount==0&&s.heartbeat){
    s.heartbeat->cancel();s.heartbeat.reset();s.startIdle();
  }
}
} // namespace batbox::flights
