#include "cli/remote/CcrRuntime.hpp"
#include "runtime/JsonValue.hpp"
#include <stdexcept>

namespace batbox::cli::remote {
namespace {
std::string field(const Json& value,const char* name){
  const auto& part=value::property(value,name);
  if(!part.is_string())throw std::invalid_argument("CCR URL field required");
  return part.get<std::string>();
}
SerialBatchConfig batchConfig(const CcrRuntimeServices& services,
    std::size_t maxSize,std::size_t maxQueue,
    std::optional<std::size_t> maxBytes={}){
  SerialBatchConfig config;
  config.maxBatchSize=maxSize;config.maxQueueSize=maxQueue;
  config.maxBatchBytes=maxBytes;
  config.baseDelayMs=500;config.maxDelayMs=30000;config.jitterMs=500;
  config.randomUnit=services.randomUnit;
  config.jsonStringify=[](const Json& value){return value::stringify(value);};
  return config;
}
}
CcrRuntime::CcrRuntime(event_loop::Loop& loop,
    std::shared_ptr<SSETransport> transport,const Json& flightUrl,
    CcrRuntimeServices services):loop_(loop),transport_(std::move(transport)),
    services_(std::move(services)){
  if(!transport_||!services_.http||!services_.getAuthHeaders||
     !services_.getFlightIngressAuthToken||!services_.decodeJwtExpiry||
     !services_.getEnvironmentWorkerEpoch||!services_.userBat||
     !services_.randomUUID||!services_.nowMs||!services_.randomUnit||
     !services_.registerFlightActivity||
     !services_.unregisterFlightActivity||!services_.onEpochMismatch||
     !services_.logDebug||services_.protocolVersionHeaderName.empty())
    throw std::invalid_argument("CCR runtime imports required");
  const auto protocol=field(flightUrl,"protocol");
  if(protocol!="http:"&&protocol!="https:")
    throw std::invalid_argument("CCRClient: Expected http(s) URL, got "+
      protocol);
  auto pathname=field(flightUrl,"pathname");
  if(!pathname.empty()&&pathname.back()=='/')pathname.pop_back();
  flightBaseUrl_=protocol+"//"+field(flightUrl,"host")+pathname;
  const auto slash=pathname.rfind('/');
  flightId_=slash==std::string::npos?pathname:pathname.substr(slash+1);
  WorkerStateConfig worker;
  worker.baseDelayMs=500;worker.maxDelayMs=30000;worker.jitterMs=500;
  worker.randomUnit=services_.randomUnit;
  worker.send=[this](const Json& body){
    Json payload={{"worker_epoch",workerEpoch_}};
    if(body.is_object())for(auto it=body.begin();it!=body.end();++it)
      payload[it.key()]=it.value();
    auto requestResult=request("put","/worker",payload,"PUT worker");
    Deferred<bool> answer(loop_);
    requestResult.then([answer](const CcrRequestResult* value,
      const std::exception_ptr& error){
      if(error)answer.reject(error);
      else answer.resolve(value&&value->ok);
    });
    return answer;
  };
  workerState_=std::make_shared<WorkerStateUploader>(loop_,std::move(worker));
  auto client=batchConfig(services_,100,100000,10*1024*1024);
  client.send=[this](const std::vector<Json>& batch){
    auto requestResult=request("post","/worker/events",
      Json{{"worker_epoch",workerEpoch_},{"events",batch}},
      "client events");
    Deferred<Unit> answer(loop_);
    requestResult.then([answer](const CcrRequestResult* value,
      const std::exception_ptr& error){
      if(error){answer.reject(error);return;}
      if(value&&value->ok){answer.resolve({});return;}
      answer.reject(std::make_exception_ptr(RetryableError(
        "client event POST failed",value?value->retryAfterMs:std::nullopt)));
    });
    return answer;
  };
  eventUploader_=std::make_shared<SerialBatchEventUploader>(loop_,
    std::move(client));
  auto internal=batchConfig(services_,100,200,10*1024*1024);
  internal.send=[this](const std::vector<Json>& batch){
    auto requestResult=request("post","/worker/internal-events",
      Json{{"worker_epoch",workerEpoch_},{"events",batch}},
      "internal events");
    Deferred<Unit> answer(loop_);
    requestResult.then([answer](const CcrRequestResult* value,
      const std::exception_ptr& error){
      if(error){answer.reject(error);return;}
      if(value&&value->ok){answer.resolve({});return;}
      answer.reject(std::make_exception_ptr(RetryableError(
        "internal event POST failed",
        value?value->retryAfterMs:std::nullopt)));
    });
    return answer;
  };
  internalEventUploader_=std::make_shared<SerialBatchEventUploader>(loop_,
    std::move(internal));
  auto delivery=batchConfig(services_,64,64);
  delivery.send=[this](const std::vector<Json>& batch){
    Json updates=Json::array();
    for(const auto& entry:batch)
      updates.push_back(Json{{"event_id",value::property(entry,"eventId")},
        {"status",value::property(entry,"status")}});
    auto requestResult=request("post","/worker/events/delivery",
      Json{{"worker_epoch",workerEpoch_},{"updates",updates}},
      "delivery batch");
    Deferred<Unit> answer(loop_);
    requestResult.then([answer](const CcrRequestResult* value,
      const std::exception_ptr& error){
      if(error){answer.reject(error);return;}
      if(value&&value->ok){answer.resolve({});return;}
      answer.reject(std::make_exception_ptr(RetryableError(
        "delivery POST failed",value?value->retryAfterMs:std::nullopt)));
    });
    return answer;
  };
  deliveryUploader_=std::make_shared<SerialBatchEventUploader>(loop_,
    std::move(delivery));
}
void CcrRuntime::attachTransportEvents(){
  const std::weak_ptr<CcrRuntime> weak=shared_from_this();
  transport_->setOnEvent([weak](const StreamClientEvent& event){
    if(const auto self=weak.lock()){
      const auto& id=value::property(event.value,"event_id");
      if(id.is_string())self->reportDelivery(id.get<std::string>(),"received");
    }
  });
}
std::shared_ptr<CcrClient> CcrRuntime::asCcrClient(){
  attachTransportEvents();
  const auto self=shared_from_this();
  auto interface=std::make_shared<CcrClient>();
  interface->initialize=[self]{return self->initialize();};
  interface->writeEvent=[self](const Json& message){
    return self->writeEvent(message);
  };
  interface->flushInternalEvents=[self]{return self->flushInternalEvents();};
  interface->internalEventsPending=[self]{
    return self->internalEventsPending();
  };
  interface->close=[self]{
    self->close();Deferred<Unit> done(self->loop_);done.resolve({});return done;
  };
  interface->writeInternalEvent=[self](const std::string& type,
      const Json& payload,const Json& options){
    Deferred<Json> answer(self->loop_);
    auto pending=self->writeInternalEvent(type,payload,
      value::property(options,"isCompaction")==true,
      value::property(options,"agentId").is_string()?std::optional<std::string>(
        value::property(options,"agentId").get<std::string>()):std::nullopt);
    pending.then([answer](const Unit*,const std::exception_ptr& error){
      if(error)answer.reject(error);else answer.resolve(nullptr);
    });
    return answer;
  };
  interface->readInternalEvents=[self]{return self->readInternalEvents();};
  interface->readSubagentInternalEvents=[self]{
    return self->readSubagentInternalEvents();
  };
  interface->reportDelivery=[self](const std::string& id,
      const std::string& status){self->reportDelivery(id,status);};
  interface->reportState=[self](const std::string& state,const Json& details){
    self->reportState(state,details.is_null()?std::nullopt:
      std::optional<Json>(details));
  };
  interface->reportMetadata=[self](const Json& metadata){
    self->reportMetadata(metadata);
  };
  return interface;
}
std::shared_ptr<CcrClient> makeCcrClient(event_loop::Loop& loop,
    std::shared_ptr<SSETransport> transport,const Json& flightUrl,
    CcrRuntimeServices services){
  return std::make_shared<CcrRuntime>(loop,std::move(transport),flightUrl,
    std::move(services))->asCcrClient();
}
}
