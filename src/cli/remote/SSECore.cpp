#include "cli/remote/SSETransport.hpp"
#include "runtime/JsonValue.hpp"
#include "runtime/Url.hpp"
#include "cli/remote/UrlSearch.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace batbox::cli::remote {
namespace {
std::string field(const Json& value,const char* name){
  const auto& part=value::property(value,name);
  if(!part.is_string())throw std::invalid_argument("SSE URL field required");
  return part.get<std::string>();
}
void assign(Json& destination,const Json& source){
  if(!source.is_object())return;
  for(auto it=source.begin();it!=source.end();++it)
    destination[it.key()]=it.value();
}
bool hasOwn(const Json& object,const char* key){
  return object.is_object()&&object.contains(key);
}
}
SSETransport::SSETransport(event_loop::Loop& loop,Json url,Json headers,
    std::optional<std::string> flightId,std::function<Json()> refreshHeaders,
    std::optional<double> initialSequenceNum,SseServices services):
    loop_(loop),url_(std::move(url)),headers_(std::move(headers)),
    flightId_(std::move(flightId)),
    refreshHeaders_(std::move(refreshHeaders)),services_(std::move(services)),
    postUrl_(postUrl(url_)){
  if(!headers_.is_object()||!services_.openStream||!services_.post||
     !services_.getAuthHeaders||!services_.userBat||
     !services_.nowMs||!services_.randomUnit||
     !services_.jsonStringify||!services_.logDebug||
     services_.protocolVersionHeaderName.empty())
    throw std::invalid_argument("SSE transport imports required");
  if(initialSequenceNum&&*initialSequenceNum>0)
    lastSequenceNum_=*initialSequenceNum;
  services_.logDebug("SSETransport: SSE URL = "+field(url_,"href"),nullptr);
  services_.logDebug("SSETransport: POST URL = "+postUrl_,nullptr);
}
std::string SSETransport::postUrl(const Json& url){
  auto path=field(url,"pathname");
  if(path.size()>=7&&path.compare(path.size()-7,7,"/stream")==0)
    path.erase(path.size()-7);
  return field(url,"protocol")+"//"+field(url,"host")+path;
}
std::string SSETransport::sseUrl()const{
  if(lastSequenceNum_<=0)return field(url_,"href");
  return setUrlSearchParam(field(url_,"href"),
    "from_sequence_num",value::numberToString(lastSequenceNum_));
}
Deferred<Unit> SSETransport::connect(){
  Deferred<Unit> result(loop_);
  try{
    if(state_!=State::Idle&&state_!=State::Reconnecting){
      services_.logDebug("SSETransport: Cannot connect, current state is "+
        std::string(state_==State::Connected?"connected":
          state_==State::Closing?"closing":"closed"),
        Json{{"level","error"}});
      result.resolve({});return result;
    }
    state_=State::Reconnecting;
    const auto auth=services_.getAuthHeaders();
    Json headers=headers_;
    assign(headers,auth);
    headers["Accept"]="text/event-stream";
    headers[services_.protocolVersionHeaderName]=
      services_.protocolVersionHeaderValue;
    headers["User-Agent"]=services_.userBat();
    if(hasOwn(auth,"Cookie")&&value::truthy(value::property(auth,"Cookie")))
      headers.erase("Authorization");
    if(lastSequenceNum_>0)
      headers["Last-Event-ID"]=value::numberToString(lastSequenceNum_);
    const auto target=sseUrl();
    services_.logDebug("SSETransport: Opening "+target,nullptr);
    const std::weak_ptr<SSETransport> weak=shared_from_this();
    const auto epoch=++streamEpoch_;
    SseStreamCallbacks callbacks;
    callbacks.onHeaders=[weak,epoch,result](int status,bool body){
      if(const auto self=weak.lock();self&&self->streamEpoch_==epoch)
        self->receivedHeaders(status,body,result);
    };
    callbacks.onBytes=[weak,epoch](const std::string& bytes){
      if(const auto self=weak.lock();self&&self->streamEpoch_==epoch)
        self->receivedBytes(bytes);
    };
    callbacks.onEnd=[weak,epoch,result](const auto& error){
      if(const auto self=weak.lock();self&&self->streamEpoch_==epoch)
        self->streamEnded(error,result);
    };
    stream_=services_.openStream(target,headers,std::move(callbacks));
    if(!stream_||!stream_->abort)
      throw std::invalid_argument("SSE stream handle required");
  }catch(...){
    if(state_!=State::Closing&&state_!=State::Closed)handleConnectionError();
    result.resolve({});
  }
  return result;
}
void SSETransport::receivedHeaders(int status,bool hasBody,
    const Deferred<Unit>& result){
  if(status<200||status>=300){
    services_.logDebug("SSETransport: HTTP "+std::to_string(status),
      Json{{"level","error"}});
    if(status==401||status==403||status==404){
      ++streamEpoch_;
      if(stream_)stream_->abort();
      stream_.reset();state_=State::Closed;
      if(onClose_)onClose_(status);
    }else handleConnectionError();
    result.resolve({});return;
  }
  if(!hasBody){
    services_.logDebug("SSETransport: No response body",nullptr);
    handleConnectionError();result.resolve({});return;
  }
  state_=State::Connected;
  reconnectAttempts_=0;
  reconnectStartTime_.reset();
  services_.logDebug("SSETransport: Connected",nullptr);
  resetLivenessTimer();
}
void SSETransport::receivedBytes(const std::string& bytes){
  byteBuffer_+=bytes;
  const auto end=byteBuffer_.rfind("\n\n");
  if(end==std::string::npos)return;
  const auto decoded=value::decodeUtf8(
    std::string_view(byteBuffer_).substr(0,end+2),!decoderStarted_);
  decoderStarted_=true;
  byteBuffer_.erase(0,end+2);
  for(const auto& frame:parseSSEFrames(decoded).frames)
    processFrame(frame);
}
void SSETransport::processFrame(const SSEFrame& frame){
  resetLivenessTimer();
  if(frame.id&& !frame.id->empty()){
    const double sequence=value::parseInt10(*frame.id);
    if(!std::isnan(sequence)){
      if(seenSequenceNums_.contains(sequence))
        services_.logDebug("SSETransport: DUPLICATE frame seq="+
          value::numberToString(sequence),Json{{"level","warn"}});
      else{
        seenSequenceNums_.insert(sequence);
        if(seenSequenceNums_.size()>1000){
          const auto threshold=lastSequenceNum_-200;
          for(auto it=seenSequenceNums_.begin();it!=seenSequenceNums_.end();)
            if(*it<threshold)it=seenSequenceNums_.erase(it);
            else ++it;
        }
      }
      if(sequence>lastSequenceNum_)lastSequenceNum_=sequence;
    }
  }
  if(frame.event&&frame.data&& !frame.event->empty()&& !frame.data->empty())
    handleSSEFrame(*frame.event,*frame.data);
  else if(frame.data&& !frame.data->empty())
    services_.logDebug("SSETransport: Frame has data but no event field",
      Json{{"level","warn"}});
}
void SSETransport::handleSSEFrame(const std::string& eventType,
    const std::string& data){
  if(eventType!="client_event"){
    services_.logDebug("SSETransport: Unexpected event type "+eventType,
      Json{{"level","warn"}});return;
  }
  Json event;
  try{event=value::parse(data);}catch(...){
    services_.logDebug("SSETransport: Failed to parse client event",
      Json{{"level","error"}});return;
  }
  const auto& payload=value::property(event,"payload");
  if(payload.is_object()&&payload.contains("type")){
    if(onData_)onData_(services_.jsonStringify(payload)+"\n");
  }
  if(onEvent_)onEvent_(StreamClientEvent{event});
}
void SSETransport::streamEnded(const std::optional<std::string>& error,
    const Deferred<Unit>& result){
  if(state_==State::Closing){result.resolve({});return;}
  if(error)services_.logDebug("SSETransport: Stream read error: "+*error,
    Json{{"level","error"}});
  if(state_!=State::Closed)handleConnectionError();
  result.resolve({});
}
void SSETransport::setOnData(
    std::function<void(const std::string&)> callback){onData_=std::move(callback);}
void SSETransport::setOnClose(
    std::function<void(std::optional<int>)> callback){onClose_=std::move(callback);}
void SSETransport::setOnEvent(
    std::function<void(const StreamClientEvent&)> callback){onEvent_=std::move(callback);}
bool SSETransport::isConnectedStatus()const{return state_==State::Connected;}
bool SSETransport::isClosedStatus()const{return state_==State::Closed;}
std::string SSETransport::getStateLabel()const{
  switch(state_){
    case State::Idle:return "idle";
    case State::Connected:return "connected";
    case State::Reconnecting:return "reconnecting";
    case State::Closing:return "closing";
    case State::Closed:return "closed";
  }
  return "closed";
}
std::shared_ptr<Transport> SSETransport::asTransport(){
  const auto self=shared_from_this();
  auto interface=std::make_shared<Transport>();
  interface->isSse=true;
  interface->setOnData=[self](auto callback){self->setOnData(std::move(callback));};
  interface->setOnClose=[self](auto callback){
    self->setOnClose([callback=std::move(callback)](auto){callback();});
  };
  interface->connect=[self]{return self->connect();};
  interface->write=[self](const Json& data){return self->write(data);};
  interface->close=[self]{self->close();};
  return interface;
}
std::shared_ptr<Transport> makeSseTransport(event_loop::Loop& loop,
    const Json& url,const Json& headers,const std::string& flightId,
    std::function<Json()> refreshHeaders,SseServices services,
    std::optional<double> initialSequenceNum){
  return std::make_shared<SSETransport>(loop,url,headers,flightId,
    std::move(refreshHeaders),initialSequenceNum,std::move(services))
      ->asTransport();
}
}
