#include "cli/remote/HybridTransport.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::cli::remote {
namespace {
std::string field(const Json& url,const char* name){
  const auto& value=value::property(url,name);
  if(!value.is_string())throw std::invalid_argument("Hybrid URL field required");
  return value.get<std::string>();
}
}
std::string HybridTransport::postUrl(const Json& url){
  const auto protocol=field(url,"protocol")=="wss:"?"https:":"http:";
  auto pathname=field(url,"pathname");
  const auto at=pathname.find("/ws/");
  if(at!=std::string::npos)pathname.replace(at,4,"/session/");
  if(!pathname.ends_with("/events")){
    if(!pathname.empty()&&pathname.back()!='/')pathname+='/';
    pathname+="events";
  }
  return std::string(protocol)+"//"+field(url,"host")+pathname+
    field(url,"search");
}
HybridTransport::HybridTransport(event_loop::Loop& loop,Json url,Json headers,
    std::optional<std::string> flightId,
    std::function<Json()> refreshHeaders,HybridServices services,
    WebSocketOptions options):loop_(loop),
    websocket_(std::make_shared<WebSocketTransport>(loop,url,headers,
      std::move(flightId),std::move(refreshHeaders),services.websocket,
      options)),services_(std::move(services)),postUrl_(postUrl(url)){
  if(!services_.getFlightIngressAuthToken||!services_.post||
     !services_.logDebug||!services_.randomUnit||!services_.jsonStringify)
    throw std::invalid_argument("Hybrid transport imports required");
  SerialBatchConfig upload;
  upload.maxBatchSize=500;upload.maxQueueSize=100000;
  upload.baseDelayMs=500;upload.maxDelayMs=8000;upload.jitterMs=1000;
  upload.maxConsecutiveFailures=services_.maxConsecutiveFailures;
  upload.onBatchDropped=services_.onBatchDropped;
  upload.randomUnit=services_.randomUnit;
  upload.jsonStringify=services_.jsonStringify;
  upload.send=[this](const auto& events){return postOnce(events);};
  uploader_=std::make_shared<SerialBatchEventUploader>(loop_,std::move(upload));
  services_.logDebug("HybridTransport: POST URL = "+postUrl_,nullptr);
}
Deferred<Unit> HybridTransport::connect(){return websocket_->connect();}
void HybridTransport::setOnData(
    std::function<void(const std::string&)> callback){
  websocket_->setOnData(std::move(callback));
}
void HybridTransport::setOnClose(std::function<void(int)> callback){
  websocket_->setOnClose(std::move(callback));
}
bool HybridTransport::isConnectedStatus()const{
  return websocket_->isConnectedStatus();
}
bool HybridTransport::isClosedStatus()const{
  return websocket_->isClosedStatus();
}
std::vector<Json> HybridTransport::takeStreamEvents(){
  if(streamEventTimer_.valid())streamEventTimer_.cancel();
  streamEventTimer_={};
  auto buffered=std::move(streamEventBuffer_);
  streamEventBuffer_.clear();
  return buffered;
}
void HybridTransport::flushStreamEvents(){
  streamEventTimer_={};
  uploader_->enqueue(takeStreamEvents());
}
Deferred<Unit> HybridTransport::write(const Json& message){
  Deferred<Unit> result(loop_);
  if(value::property(message,"type")=="stream_event"){
    streamEventBuffer_.push_back(message);
    if(!streamEventTimer_.valid()){
      const std::weak_ptr<HybridTransport> weak=shared_from_this();
      streamEventTimer_=loop_.setTimeout(100,[weak]{
        if(const auto self=weak.lock())self->flushStreamEvents();
      });
    }
    result.resolve({});return result;
  }
  auto batch=takeStreamEvents();batch.push_back(message);
  auto queued=uploader_->enqueue(std::move(batch));
  const std::weak_ptr<HybridTransport> weak=shared_from_this();
  queued.then([weak,result](const Unit*,const std::exception_ptr& error){
    if(error){result.reject(error);return;}
    if(const auto self=weak.lock()){
      self->uploader_->flush().then([result](const Unit*,const auto& failure){
        if(failure)result.reject(failure);else result.resolve({});
      });
    }else result.resolve({});
  });
  return result;
}
Deferred<Unit> HybridTransport::writeBatch(std::vector<Json> messages){
  Deferred<Unit> result(loop_);
  auto batch=takeStreamEvents();
  for(auto& message:messages)batch.push_back(std::move(message));
  auto queued=uploader_->enqueue(std::move(batch));
  const std::weak_ptr<HybridTransport> weak=shared_from_this();
  queued.then([weak,result](const Unit*,const auto& error){
    if(error){result.reject(error);return;}
    if(const auto self=weak.lock()){
      self->uploader_->flush().then([result](const Unit*,const auto& failure){
        if(failure)result.reject(failure);else result.resolve({});
      });
    }else result.resolve({});
  });
  return result;
}
Deferred<Unit> HybridTransport::flush(){
  uploader_->enqueue(takeStreamEvents());
  return uploader_->flush();
}
std::size_t HybridTransport::droppedBatchCount()const{
  return uploader_->droppedBatchCount();
}
void HybridTransport::close(){
  if(closed_)return;
  closed_=true;
  if(streamEventTimer_.valid())streamEventTimer_.cancel();
  streamEventTimer_={};streamEventBuffer_.clear();
  const std::weak_ptr<HybridTransport> weak=shared_from_this();
  auto remaining=uploader_->flush();
  const auto stop=[weak]{
    if(const auto self=weak.lock()){
      if(self->graceTimer_.valid())self->graceTimer_.cancel();
      self->graceTimer_={};self->uploader_->close();
    }
  };
  remaining.then([stop](const Unit*,const auto&){stop();});
  graceTimer_=loop_.setTimeout(3000,stop);
  websocket_->close();
}
std::shared_ptr<Transport> HybridTransport::asTransport(){
  const auto self=shared_from_this();
  auto out=std::make_shared<Transport>();
  out->setOnData=[self](auto callback){self->setOnData(std::move(callback));};
  out->setOnClose=[self](auto callback){
    self->setOnClose([callback=std::move(callback)](int){callback();});
  };
  out->connect=[self]{return self->connect();};
  out->write=[self](const Json& message){return self->write(message);};
  out->close=[self]{self->close();};
  return out;
}
std::shared_ptr<Transport> makeHybridTransport(event_loop::Loop& loop,
    const Json& url,const Json& headers,const std::string& flightId,
    std::function<Json()> refreshHeaders,HybridServices services,
    WebSocketOptions options){
  return std::make_shared<HybridTransport>(loop,url,headers,flightId,
    std::move(refreshHeaders),std::move(services),options)->asTransport();
}
}
