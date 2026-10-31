#include "cli/remote/WebSocketTransport.hpp"
#include "runtime/JsonValue.hpp"
#include <cmath>

namespace batbox::cli::remote {
void WebSocketTransport::stopTimers(){
  if(pingTimer_.valid())pingTimer_.cancel();
  if(keepAliveTimer_.valid())keepAliveTimer_.cancel();
}
void WebSocketTransport::schedulePing(){
  if(pingTimer_.valid())pingTimer_.cancel();
  const std::weak_ptr<WebSocketTransport> weak=shared_from_this();
  pingTimer_=loop_.setTimeout(10000,[weak]{
    const auto self=weak.lock();
    if(!self||self->state_!=State::Connected||!self->socket_)return;
    const double now=self->services_.nowMs();
    const double gap=now-self->lastPingTick_;
    self->lastPingTick_=now;
    if(gap>60000){
      self->services_.logDebug("WebSocketTransport: "+
        value::numberToString(std::floor(gap/1000+0.5))+
        "s tick gap detected, forcing reconnect",nullptr);
      self->connectionError();return;
    }
    if(!self->pongReceived_){
      self->services_.logDebug("WebSocketTransport: No pong received, "
        "connection appears dead",Json{{"level","error"}});
      self->connectionError();return;
    }
    self->pongReceived_=false;
    try{if(self->socket_->ping)self->socket_->ping();}
    catch(const std::exception& error){
      self->services_.logDebug("WebSocketTransport: Ping failed: "+
        std::string(error.what()),Json{{"level","error"}});
    }
    self->schedulePing();
  });
}
void WebSocketTransport::scheduleKeepAlive(){
  if(keepAliveTimer_.valid())keepAliveTimer_.cancel();
  if(services_.remoteMode)return;
  const std::weak_ptr<WebSocketTransport> weak=shared_from_this();
  keepAliveTimer_=loop_.setTimeout(300000,[weak]{
    const auto self=weak.lock();
    if(!self||self->state_!=State::Connected||!self->socket_)return;
    try{
      self->socket_->send("{\"type\":\"keep_alive\"}\n");
      self->lastActivityTime_=self->services_.nowMs();
      self->services_.logDebug("WebSocketTransport: Sent periodic "
        "keep_alive data frame",nullptr);
    }catch(const std::exception& error){
      self->services_.logDebug("WebSocketTransport: Periodic keep_alive "
        "failed: "+std::string(error.what()),Json{{"level","error"}});
    }
    self->scheduleKeepAlive();
  });
}
}
