#include "cli/structured/StructuredFlight.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::cli::structured {
StructuredFlight::StructuredFlight(event_loop::Loop& loop,
    StructuredFlightServices services):loop_(loop),services_(std::move(services)){
  if(!services_.nextChunk||!services_.writeOutput||!services_.randomUUID||
     !services_.logHookError)
    throw std::invalid_argument("Structured IO imports required");
  control_=std::make_shared<ControlResponses>([this](const Json& message){
    // Control responses are queued without awaiting the write.
    write(message);
  });
  outbound_=std::make_shared<AsyncStream<Json>>(loop_);
  RequestProducerServices requestServices;
  requestServices.randomUUID=services_.randomUUID;
  requestServices.onControlRequestSent=[this](const Json& message){
    if(onControlRequestSent_)onControlRequestSent_(message);
  };
  producer_=std::make_shared<RequestProducer>(loop_,control_,outbound_,
    std::move(requestServices));
  InputReaderServices inputServices;
  inputServices.nextChunk=services_.nextChunk;
  // Parse messages without collecting their types for diagnostics.
  inputServices.logParsedMessageType=[](const std::string&){};
  inputServices.processLine=services_.processLine;
  inputServices.processLine.replayUserMessages=services_.replayUserMessages;
  inputServices.processLine.unexpectedResponseCallback=[this](const Json& value){
    if(unexpectedResponseCallback_)return unexpectedResponseCallback_(value);
    event_loop::Deferred<event_loop::Unit> result(loop_);
    result.resolve({});return result;
  };
  inputServices.processLine.onControlRequestResolved=[this](
      const std::string& id){
    if(onControlRequestResolved_)onControlRequestResolved_(id);
  };
  reader_=std::make_shared<InputReader>(loop_,control_,producer_,
    std::move(inputServices));
  adapters_.randomUUID=services_.randomUUID;
  adapters_.logHookError=services_.logHookError;
  adapters_.schemas=makeIndigenousControlSchemas(services_.schemas);
}
event_loop::Deferred<StreamItem<Json>> StructuredFlight::nextInput(){
  return reader_->next();
}
event_loop::Deferred<StreamItem<Json>> StructuredFlight::nextOutbound(){
  return outbound_->next();
}
void StructuredFlight::prependUserMessage(const std::string& content){
  reader_->prependUserMessage(content);
}
event_loop::Deferred<event_loop::Unit> StructuredFlight::write(
    const Json& message){return services_.writeOutput(message);}
event_loop::Deferred<event_loop::Unit> StructuredFlight::flushInternalEvents(){
  event_loop::Deferred<event_loop::Unit> result(loop_);result.resolve({});
  return result;
}
event_loop::Deferred<Json> StructuredFlight::restoredWorkerState()const{
  event_loop::Deferred<Json> result(loop_);result.resolve(nullptr);return result;
}
Json StructuredFlight::getPendingPermissionRequests()const{
  return control_->getPendingPermissionRequests();
}
void StructuredFlight::injectControlResponse(const Json& response){
  control_->injectControlResponse(response);
}
void StructuredFlight::setUnexpectedResponseCallback(
    std::function<event_loop::Deferred<event_loop::Unit>(const Json&)> callback){
  unexpectedResponseCallback_=std::move(callback);
}
void StructuredFlight::setOnControlRequestSent(
    std::function<void(const Json&)> callback){
  onControlRequestSent_=std::move(callback);
}
void StructuredFlight::setOnControlRequestResolved(
    std::function<void(const std::string&)> callback){
  onControlRequestResolved_=std::move(callback);
}
permissions::PermissionRaceServices StructuredFlight::permissionServices(
    permissions::PermissionRaceServices services)const{
  services.randomUUID=services_.randomUUID;
  services.parsePermissionGadgetOutput=adapters_.schemas.permissionOutput;
  return services;
}
event_loop::Deferred<Json> StructuredFlight::createCanUseGadget(
    const gadgets::Gadget& gadget,const Json& input,
    const permissions::PermissionUseContext& context,
    const Json& assistantMessage,const std::string& gadgetUseID,
    const std::optional<Json>& forceDecision,
    const permissions::PermissionRaceServices& services,
    const permissions::PermissionPoetryCallback& onPermissionPoetry){
  return permissions::createCanUseGadget(loop_,*producer_,*control_,gadget,input,
    context,assistantMessage,gadgetUseID,forceDecision,
    permissionServices(services),onPermissionPoetry);
}
HookCallback StructuredFlight::createHookCallback(
    const std::string& callbackId,const std::optional<double>& timeout){
  return structured::createHookCallback(loop_,producer_,callbackId,timeout,
    adapters_);
}
event_loop::Deferred<Json> StructuredFlight::handleElicitation(
    const std::string& serverName,const std::string& message,
    const std::optional<Json>& requestedSchema,std::stop_token signal,
    const std::optional<std::string>& mode,const std::optional<std::string>& url,
    const std::optional<std::string>& elicitationId){
  return structured::handleElicitation(loop_,producer_,serverName,message,
    requestedSchema,signal,mode,url,elicitationId,adapters_);
}
std::function<event_loop::Deferred<bool>(const Json&)>
StructuredFlight::createSandboxAskCallback(){
  return structured::createSandboxAskCallback(loop_,producer_,adapters_);
}
event_loop::Deferred<Json> StructuredFlight::sendMcpMessage(
    const std::string& serverName,const Json& message){
  return structured::sendMcpMessage(loop_,producer_,serverName,message,
    adapters_);
}
}
