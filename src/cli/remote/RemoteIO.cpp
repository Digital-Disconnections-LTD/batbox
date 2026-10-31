#include "cli/remote/RemoteIO.hpp"
#include "runtime/Url.hpp"
#include "utils/envUtils.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::cli::remote {
Deferred<std::optional<std::string>> RemoteIO::Input::next(){
    Deferred<std::optional<std::string>> result(loop);
    if(!chunks.empty()){
      auto chunk=std::move(chunks.front());chunks.pop_front();
      result.resolve(std::move(chunk));
    }else if(ended)result.resolve({});
    else waiting=result;
    return result;
}
void RemoteIO::Input::write(std::string data){
    if(ended)throw std::runtime_error("Stream closed");
    if(waiting){auto result=std::move(*waiting);waiting.reset();
      result.resolve(std::move(data));}
    else chunks.push_back(std::move(data));
}
void RemoteIO::Input::end(){
    if(ended)return;
    ended=true;
    if(waiting){auto result=std::move(*waiting);waiting.reset();
      result.resolve({});}
}
namespace {
std::string errorMessage(std::exception_ptr failure){
  try{if(failure)std::rethrow_exception(failure);}
  catch(const std::exception& error){return error.what();}
  catch(...){return "unknown";}
  return "unknown";
}
bool envTruthy(const Services& services,const std::string& key){
  const auto value=services.environment(key);
  return batbox::isEnvTruthy(value?Json(*value):Json());
}
}
Json RemoteIO::refreshedHeaders()const{
  Json headers=Json::object();
  const auto token=services_.getFlightIngressAuthToken();
  if(token&& !token->empty())headers["Authorization"]="Bearer "+*token;
  const auto version=services_.environment(services_.environmentRunnerVersionKey);
  if(version&&!version->empty())
    headers["x-environment-runner-version"]=*version;
  return headers;
}
RemoteIO::RemoteIO(event_loop::Loop& loop,const std::string& streamUrl,
    Services services,bool replayUserMessages):loop_(loop),
    services_(std::move(services)),input_(std::make_shared<Input>(loop)),
    restoredWorkerState_(loop){
  if(!services_.getFlightIngressAuthToken||!services_.getFlightId||
     !services_.environment||!services_.isDebugMode||
     !services_.keepAliveIntervalMs||!services_.getTransportForUrl||
     !services_.registerCleanup||!services_.logForDebugging||
     !services_.writeToStdout||!services_.ndjsonSafeStringify)
    throw std::invalid_argument("Remote IO imports required");
  auto structuredServices=services_.structured;
  const auto input=input_;
  structuredServices.nextChunk=[input]{return input->next();};
  structuredServices.writeOutput=[this](const Json& message){
    return write(message);
  };
  structuredServices.replayUserMessages=replayUserMessages;
  structured_=std::make_unique<structured::StructuredFlight>(
    loop_,std::move(structuredServices));
  auto url=runtime::parseUrl(streamUrl);
  if(!url)throw std::invalid_argument("Invalid remote URL");
  Json headers=Json::object();
  const auto token=services_.getFlightIngressAuthToken();
  if(token&&!token->empty())headers["Authorization"]="Bearer "+*token;
  else services_.logForDebugging(
    "[remote-io] No session ingress token available",
    Json{{"level","error"}});
  const auto version=services_.environment(services_.environmentRunnerVersionKey);
  if(version&&!version->empty())
    headers["x-environment-runner-version"]=*version;
  const std::weak_ptr<bool> alive=alive_;
  transport_=services_.getTransportForUrl(*url,headers,
    services_.getFlightId(),[this,alive]{
      if(alive.expired())return Json::object();
      return refreshedHeaders();
    });
  if(!transport_||!transport_->setOnData||!transport_->setOnClose||
     !transport_->connect||!transport_->write||!transport_->close)
    throw std::invalid_argument("Remote transport import required");
  bridge_=services_.environment(services_.environmentKindKey)=="bridge";
  debug_=services_.isDebugMode();
  const auto dispatcher=loop_.dispatcher();
  const auto bridge=bridge_,debug=debug_;
  const auto writeStdout=services_.writeToStdout;
  transport_->setOnData([input,dispatcher,bridge,debug,writeStdout](
      const std::string& data){
    auto received=[input,bridge,debug,writeStdout,data]{
      input->write(data);
      if(bridge&&debug)
        writeStdout(!data.empty()&&data.back()=='\n'?data:data+"\n");
    };
    if(dispatcher.isOwnerThread())received();
    else dispatcher.postMission(std::move(received));
  });
  transport_->setOnClose([input,dispatcher]{
    if(dispatcher.isOwnerThread())input->end();
    else dispatcher.postMission([input]{input->end();});
  });
  if(envTruthy(services_,services_.ccrV2Key)){
    if(!transport_->isSse)
      throw std::runtime_error("CCR v2 requires SSETransport; check getTransportForUrl");
    if(!services_.createCcrClient||!services_.setInternalEventWriter||
       !services_.setInternalEventReaders||
       !services_.setCommandLifecycleListener||
       !services_.setFlightStateChangedListener||
       !services_.setFlightMetadataChangedListener||
       !services_.logError||!services_.gracefulShutdown)
      throw std::invalid_argument("Remote event client imports required");
    ccr_=services_.createCcrClient(*transport_,*url);
    if(!ccr_||!ccr_->initialize||!ccr_->writeEvent||
       !ccr_->flushInternalEvents||!ccr_->internalEventsPending||
       !ccr_->close||!ccr_->writeInternalEvent||
       !ccr_->readInternalEvents||!ccr_->readSubagentInternalEvents||
       !ccr_->reportDelivery||!ccr_->reportState||!ccr_->reportMetadata)
      throw std::invalid_argument("Complete remote event client required");
    const auto init=ccr_->initialize();
    init.then([output=restoredWorkerState_](const Json* value,
                                             const std::exception_ptr& error){
      output.resolve(error?Json():*value);
    });
    init.then([services=services_](const Json*,const std::exception_ptr& error){
      if(!error)return;
      services.logError("CCRClient initialization failed: "+errorMessage(error));
      services.gracefulShutdown(1,"other");
    });
    const auto ccr=ccr_;
    services_.registerCleanup([ccr]{return ccr->close();});
    services_.setInternalEventWriter([ccr](const std::string& type,
        const Json& payload,const Json& options){
      return ccr->writeInternalEvent(type,payload,options);
    });
    services_.setInternalEventReaders(
      [ccr]{return ccr->readInternalEvents();},
      [ccr]{return ccr->readSubagentInternalEvents();});
    services_.setCommandLifecycleListener([ccr](const std::string& id,
        const std::string& state){
      ccr->reportDelivery(id,state=="started"?"processing":"processed");
    });
    services_.setFlightStateChangedListener([ccr](
        const std::string& state,const Json& details){
      ccr->reportState(state,details);
    });
    services_.setFlightMetadataChangedListener([ccr](const Json& metadata){
      ccr->reportMetadata(metadata);
    });
  }else restoredWorkerState_.resolve(nullptr);
  transport_->connect();
  const double interval=services_.keepAliveIntervalMs();
  if(bridge_&&interval>0)scheduleKeepAlive(interval);
  services_.registerCleanup([this,alive]{
    Deferred<Unit> done(loop_);
    if(!alive.expired())close();
    done.resolve({});return done;
  });
  if(services_.nextInitialPoetryChunk)beginInitialPoetry();
}
RemoteIO::~RemoteIO(){*alive_=false;close();}
}
