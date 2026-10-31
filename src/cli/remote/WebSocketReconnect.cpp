#include "cli/remote/WebSocketTransport.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <cmath>
#include <utility>

namespace batbox::cli::remote {
namespace {
std::string href(const Json& value){
  const auto& field=value::property(value,"href");
  return field.is_string()?field.get<std::string>():std::string();
}
void assign(Json& target,const Json& fresh){
  if(!fresh.is_object())return;
  for(auto it=fresh.begin();it!=fresh.end();++it)target[it.key()]=it.value();
}
}
void WebSocketTransport::doDisconnect(){
  stopTimers();
  services_.unregisterFlightActivity();
  if(socket_){
    socket_->removeListeners();
    socket_->close();
    socket_.reset();
  }
}
void WebSocketTransport::connectionError(std::optional<int> code){
  services_.logDebug("WebSocketTransport: Disconnected from "+href(url_)+
    (code?" (code "+std::to_string(*code)+")":""),nullptr);
  doDisconnect();
  if(state_==State::Closing||state_==State::Closed)return;
  bool refreshed=false;
  if(code==4003&&refreshHeaders_){
    const auto fresh=refreshHeaders_();
    if(value::property(fresh,"Authorization")!=
       value::property(headers_,"Authorization")){
      assign(headers_,fresh);refreshed=true;
      services_.logDebug("WebSocketTransport: 4003 received but headers "
        "refreshed, scheduling reconnect",nullptr);
    }
  }
  if(code&&(*code==1002||*code==4001||*code==4003)&&!refreshed){
    services_.logDebug("WebSocketTransport: Permanent close code "+
      std::to_string(*code)+", not reconnecting",Json{{"level","error"}});
    state_=State::Closed;
    if(onClose_)onClose_(*code);
    return;
  }
  if(!options_.autoReconnect){
    state_=State::Closed;
    if(onClose_)onClose_(code.value_or(0));
    return;
  }
  const double now=services_.nowMs();
  if(!reconnectStartTime_||*reconnectStartTime_==0)reconnectStartTime_=now;
  if(lastReconnectAttemptTime_&&
     now-*lastReconnectAttemptTime_>60000){
    services_.logDebug("WebSocketTransport: Detected system sleep, "
      "resetting reconnection budget",nullptr);
    reconnectStartTime_=now;reconnectAttempts_=0;
  }
  lastReconnectAttemptTime_=now;
  const double elapsed=now-*reconnectStartTime_;
  if(elapsed<600000){
    if(reconnectTimer_.valid())reconnectTimer_.cancel();
    if(!refreshed&&refreshHeaders_){
      assign(headers_,refreshHeaders_());
      services_.logDebug("WebSocketTransport: Refreshed headers for reconnect",
        nullptr);
    }
    state_=State::Reconnecting;
    ++reconnectAttempts_;
    const double base=std::min(1000.0*std::pow(2.0,reconnectAttempts_-1),
      30000.0);
    const double delay=std::max(0.0,
      base+base*0.25*(2*services_.randomUnit()-1));
    services_.logDebug("WebSocketTransport: Reconnecting in "+
      value::numberToString(std::floor(delay+0.5))+"ms (attempt "+
      std::to_string(reconnectAttempts_)+", "+
      value::numberToString(std::floor(elapsed/1000+0.5))+"s elapsed)",nullptr);
    const std::weak_ptr<WebSocketTransport> weak=shared_from_this();
    reconnectTimer_=loop_.setTimeout(delay,[weak]{
      if(const auto self=weak.lock()){
        self->reconnectTimer_={};
        self->connect();
      }
    });
  }else{
    services_.logDebug("WebSocketTransport: Reconnection time budget "
      "exhausted after "+value::numberToString(std::floor(elapsed/1000+0.5))+
      "s for "+href(url_),Json{{"level","error"}});
    state_=State::Closed;
    if(onClose_)onClose_(code.value_or(0));
  }
}
void WebSocketTransport::close(){
  if(reconnectTimer_.valid())reconnectTimer_.cancel();
  stopTimers();
  services_.unregisterFlightActivity();
  state_=State::Closing;
  doDisconnect();
}
void WebSocketTransport::replayBufferedMessages(const std::string& lastId){
  const auto messages=messageBuffer_;
  if(messages.empty())return;
  std::size_t start=0;
  if(!lastId.empty()){
    for(std::size_t i=0;i<messages.size();++i)
      if(value::property(messages[i],"uuid")==lastId){start=i+1;break;}
    if(start>0){
      messageBuffer_.erase(messageBuffer_.begin(),
        messageBuffer_.begin()+static_cast<std::ptrdiff_t>(start));
      if(messageBuffer_.empty())lastSentId_.reset();
      services_.logDebug("WebSocketTransport: Evicted "+
        std::to_string(start)+" confirmed messages, "+
        std::to_string(messageBuffer_.size())+" remaining",nullptr);
    }
  }
  if(start==messages.size()){
    services_.logDebug("WebSocketTransport: No new messages to replay",nullptr);
    return;
  }
  services_.logDebug("WebSocketTransport: Replaying "+
    std::to_string(messages.size()-start)+" buffered messages",nullptr);
  for(std::size_t i=start;i<messages.size();++i){
    if(!sendLine(services_.jsonStringify(messages[i])+"\n")){
      connectionError();break;
    }
  }
}
}
