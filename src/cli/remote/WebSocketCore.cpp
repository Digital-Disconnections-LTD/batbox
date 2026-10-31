#include "cli/remote/WebSocketTransport.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::cli::remote {
namespace {
std::string urlHref(const Json& url){
  const auto& href=value::property(url,"href");
  if(!href.is_string())throw std::invalid_argument("WebSocket URL required");
  return href.get<std::string>();
}
std::string stringField(const Json& value,const char* key){
  const auto& field=value::property(value,key);
  return field.is_string()?field.get<std::string>():"undefined";
}
std::string controlDetailLabel(const Json& message){
  const auto& type=value::property(message,"type");
  if(type=="control_request"){
    const auto& request=value::property(message,"request");
    const auto subtype=stringField(request,"subtype");
    const auto& gadgetField=value::property(request,"tool_name");
    const auto gadget=subtype=="can_use_tool"&&value::truthy(gadgetField)?
      stringField(request,"tool_name"):std::string();
    return " subtype="+subtype+" request_id="+
      stringField(message,"request_id")+
      (gadget.empty()?std::string():" tool="+gadget);
  }
  if(type=="control_response"){
    const auto& response=value::property(message,"response");
    return " subtype="+stringField(response,"subtype")+
      " request_id="+stringField(response,"request_id");
  }
  return {};
}
}
WebSocketTransport::WebSocketTransport(event_loop::Loop& loop,Json url,
    Json headers,std::optional<std::string> flightId,
    std::function<Json()> refreshHeaders,WebSocketServices services,
    WebSocketOptions options):loop_(loop),url_(std::move(url)),
    headers_(std::move(headers)),flightId_(std::move(flightId)),
    refreshHeaders_(std::move(refreshHeaders)),services_(std::move(services)),
    options_(options){
  if(!services_.open||!services_.nowMs||!services_.randomUnit||
     !services_.logDebug||!services_.registerFlightActivity||
     !services_.unregisterFlightActivity||!services_.jsonStringify)
    throw std::invalid_argument("WebSocket transport imports required");
  if(!headers_.is_object())throw std::invalid_argument("Headers required");
  (void)urlHref(url_);
}
Deferred<Unit> WebSocketTransport::connect(){
  Deferred<Unit> output(loop_);
  try{
    if(state_!=State::Idle&&state_!=State::Reconnecting){
      services_.logDebug("WebSocketTransport: Cannot connect, current state is "+
        getStateLabel(),Json{{"level","error"}});
      output.resolve({});return output;
    }
    state_=State::Reconnecting;
    connectStartTime_=services_.nowMs();
    services_.logDebug("WebSocketTransport: Opening "+urlHref(url_),nullptr);
    Json headers=headers_;
    if(lastSentId_){
      headers["X-Last-Request-Id"]=*lastSentId_;
      services_.logDebug(
        "WebSocketTransport: Adding X-Last-Request-Id header: "+
        *lastSentId_,nullptr);
    }
    const std::weak_ptr<WebSocketTransport> weak=shared_from_this();
    WebSocketCallbacks callbacks;
    callbacks.onOpen=[weak](const std::optional<std::string>& lastId){
      if(const auto self=weak.lock())self->opened(lastId);
    };
    callbacks.onMessage=[weak](const std::string& data){
      if(const auto self=weak.lock())self->received(data);
    };
    callbacks.onError=[weak](const std::string& error){
      if(const auto self=weak.lock())
        self->services_.logDebug("WebSocketTransport: Error: "+error,
          Json{{"level","error"}});
    };
    callbacks.onClose=[weak](int code){
      if(const auto self=weak.lock())self->connectionError(code);
    };
    callbacks.onPong=[weak]{
      if(const auto self=weak.lock())self->pongReceived_=true;
    };
    socket_=services_.open(urlHref(url_),headers,std::move(callbacks));
    if(!socket_||!socket_->send||!socket_->close||
       !socket_->removeListeners)
      throw std::invalid_argument("WebSocket connection import required");
    output.resolve({});
  }catch(...){output.reject(std::current_exception());}
  return output;
}
void WebSocketTransport::opened(const std::optional<std::string>& lastId){
  const auto openedSocket=socket_;
  services_.logDebug("WebSocketTransport: Connected",nullptr);
  reconnectAttempts_=0;
  reconnectStartTime_.reset();
  lastReconnectAttemptTime_.reset();
  lastActivityTime_=services_.nowMs();
  state_=State::Connected;
  if(onConnect_)onConnect_();
  pongReceived_=true;
  lastPingTick_=services_.nowMs();
  schedulePing();
  scheduleKeepAlive();
  const std::weak_ptr<WebSocketTransport> weak=shared_from_this();
  services_.registerFlightActivity([weak]{
    if(const auto self=weak.lock())
      self->write(Json{{"type","keep_alive"}});
  });
  if(openedSocket&&lastId)replayBufferedMessages(*lastId);
}
void WebSocketTransport::received(const std::string& data){
  lastActivityTime_=services_.nowMs();
  if(onData_)onData_(data);
}
bool WebSocketTransport::sendLine(const std::string& line){
  if(!socket_||state_!=State::Connected){
    services_.logDebug("WebSocketTransport: Not connected",nullptr);
    return false;
  }
  try{
    socket_->send(line);
    lastActivityTime_=services_.nowMs();
    return true;
  }catch(const std::exception& error){
    services_.logDebug("WebSocketTransport: Failed to send: "+
      std::string(error.what()),Json{{"level","error"}});
    connectionError();
    return false;
  }
}
Deferred<Unit> WebSocketTransport::write(const Json& message){
  Deferred<Unit> output(loop_);
  try{
    const auto& uuid=value::property(message,"uuid");
    if(uuid.is_string()){
      if(messageBuffer_.size()==1000)messageBuffer_.pop_front();
      messageBuffer_.push_back(message);
      lastSentId_=uuid.get<std::string>();
    }
    const auto line=services_.jsonStringify(message)+"\n";
    if(state_==State::Connected){
      const auto flight=flightId_?" session="+*flightId_:std::string();
      services_.logDebug("WebSocketTransport: Sending message type="+
        stringField(message,"type")+flight+controlDetailLabel(message),
        nullptr);
      sendLine(line);
    }
    output.resolve({});
  }catch(...){output.reject(std::current_exception());}
  return output;
}
void WebSocketTransport::setOnData(
    std::function<void(const std::string&)> callback){
  onData_=std::move(callback);
}
void WebSocketTransport::setOnClose(std::function<void(int)> callback){
  onClose_=std::move(callback);
}
void WebSocketTransport::setOnConnect(std::function<void()> callback){
  onConnect_=std::move(callback);
}
bool WebSocketTransport::isConnectedStatus()const{
  return state_==State::Connected;
}
bool WebSocketTransport::isClosedStatus()const{return state_==State::Closed;}
std::string WebSocketTransport::getStateLabel()const{
  switch(state_){
    case State::Idle:return "idle";
    case State::Connected:return "connected";
    case State::Reconnecting:return "reconnecting";
    case State::Closing:return "closing";
    case State::Closed:return "closed";
  }
  return "closed";
}
std::shared_ptr<Transport> WebSocketTransport::asTransport(){
  auto target=std::make_shared<Transport>();
  const auto self=shared_from_this();
  target->setOnData=[self](auto callback){self->setOnData(std::move(callback));};
  target->setOnClose=[self](auto callback){
    self->setOnClose([callback=std::move(callback)](int){callback();});
  };
  target->connect=[self]{return self->connect();};
  target->write=[self](const Json& value){return self->write(value);};
  target->close=[self]{self->close();};
  return target;
}
std::shared_ptr<Transport> makeWebSocketTransport(
    event_loop::Loop& loop,const Json& url,const Json& headers,
    const std::string& flightId,std::function<Json()> refreshHeaders,
    WebSocketServices services,WebSocketOptions options){
  return std::make_shared<WebSocketTransport>(loop,url,headers,flightId,
    std::move(refreshHeaders),std::move(services),options)->asTransport();
}
}
