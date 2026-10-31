#include "cli/remote/SSETransport.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <cmath>

namespace batbox::cli::remote {
void SSETransport::handleConnectionError(){
  clearLivenessTimer();
  if(state_==State::Closing||state_==State::Closed)return;
  ++streamEpoch_;
  if(stream_)stream_->abort();
  stream_.reset();byteBuffer_.clear();decoderStarted_=false;
  const auto now=services_.nowMs();
  if(!reconnectStartTime_||*reconnectStartTime_==0)reconnectStartTime_=now;
  const auto elapsed=now-*reconnectStartTime_;
  if(elapsed<600000){
    if(reconnectTimer_.valid())reconnectTimer_.cancel();
    if(refreshHeaders_){
      const auto fresh=refreshHeaders_();
      if(fresh.is_object())for(auto it=fresh.begin();it!=fresh.end();++it)
        headers_[it.key()]=it.value();
      services_.logDebug("SSETransport: Refreshed headers for reconnect",nullptr);
    }
    state_=State::Reconnecting;
    ++reconnectAttempts_;
    const auto base=std::min(1000.0*std::pow(2.0,reconnectAttempts_-1),30000.0);
    const auto delay=std::max(0.0,
      base+base*0.25*(2*services_.randomUnit()-1));
    services_.logDebug("SSETransport: Reconnecting in "+
      value::numberToString(std::floor(delay+0.5))+"ms (attempt "+
      std::to_string(reconnectAttempts_)+")",nullptr);
    const std::weak_ptr<SSETransport> weak=shared_from_this();
    reconnectTimer_=loop_.setTimeout(delay,[weak]{
      if(const auto self=weak.lock()){
        self->reconnectTimer_={};self->connect();
      }
    });
  }else{
    state_=State::Closed;
    if(onClose_)onClose_(std::nullopt);
  }
}
void SSETransport::clearLivenessTimer(){
  if(livenessTimer_.valid())livenessTimer_.cancel();
  livenessTimer_={};
}
void SSETransport::resetLivenessTimer(){
  clearLivenessTimer();
  const std::weak_ptr<SSETransport> weak=shared_from_this();
  livenessTimer_=loop_.setTimeout(45000,[weak]{
    if(const auto self=weak.lock()){
      self->livenessTimer_={};
      self->services_.logDebug("SSETransport: Liveness timeout, reconnecting",
        Json{{"level","error"}});
      self->handleConnectionError();
    }
  });
}
void SSETransport::close(){
  if(reconnectTimer_.valid())reconnectTimer_.cancel();
  reconnectTimer_={};clearLivenessTimer();
  state_=State::Closing;++streamEpoch_;
  if(stream_)stream_->abort();
  stream_.reset();
}
}
