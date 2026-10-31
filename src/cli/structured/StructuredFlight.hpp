#pragma once
#include "cli/permissions/PermissionRace.hpp"
#include "cli/structured/ControlAdapters.hpp"
#include "cli/structured/InputReader.hpp"
#include "cli/structured/IndigenousSchemas.hpp"
#include <memory>

namespace batbox::cli::structured {
struct StructuredFlightServices {
  std::function<event_loop::Deferred<std::optional<std::string>>()> nextChunk;
  std::function<event_loop::Deferred<event_loop::Unit>(const Json&)> writeOutput;
  std::function<std::string()> randomUUID;
  ProcessLineServices processLine;
  std::function<void(const std::string&,const std::string&)> logHookError;
  IndigenousSchemaServices schemas;
  bool replayUserMessages=false;
};

// Owner-thread composition of input, output and control request streams.
class StructuredFlight {
public:
  StructuredFlight(event_loop::Loop& loop,StructuredFlightServices services);
  event_loop::Deferred<StreamItem<Json>> nextInput();
  event_loop::Deferred<StreamItem<Json>> nextOutbound();
  void prependUserMessage(const std::string& content);
  event_loop::Deferred<event_loop::Unit> write(const Json& message);
  event_loop::Deferred<event_loop::Unit> flushInternalEvents();
  std::size_t internalEventsPending()const{return 0;}
  event_loop::Deferred<Json> restoredWorkerState()const;
  Json getPendingPermissionRequests()const;
  void injectControlResponse(const Json& response);
  void setUnexpectedResponseCallback(
    std::function<event_loop::Deferred<event_loop::Unit>(const Json&)> callback);
  void setOnControlRequestSent(std::function<void(const Json&)> callback);
  void setOnControlRequestResolved(std::function<void(const std::string&)> callback);
  permissions::PermissionRaceServices permissionServices(
    permissions::PermissionRaceServices services)const;
  event_loop::Deferred<Json> createCanUseGadget(
    const gadgets::Gadget& gadget,const Json& input,
    const permissions::PermissionUseContext& context,
    const Json& assistantMessage,const std::string& gadgetUseID,
    const std::optional<Json>& forceDecision,
    const permissions::PermissionRaceServices& services,
    const permissions::PermissionPoetryCallback& onPermissionPoetry={});
  HookCallback createHookCallback(const std::string& callbackId,
    const std::optional<double>& timeout={});
  event_loop::Deferred<Json> handleElicitation(
    const std::string& serverName,const std::string& message,
    const std::optional<Json>& requestedSchema={},std::stop_token signal={},
    const std::optional<std::string>& mode={},
    const std::optional<std::string>& url={},
    const std::optional<std::string>& elicitationId={});
  std::function<event_loop::Deferred<bool>(const Json&)> createSandboxAskCallback();
  event_loop::Deferred<Json> sendMcpMessage(const std::string& serverName,
    const Json& message);
  std::shared_ptr<AsyncStream<Json>> outbound()const{return outbound_;}
  std::shared_ptr<ControlResponses> control()const{return control_;}
  std::shared_ptr<RequestProducer> producer()const{return producer_;}
private:
  event_loop::Loop& loop_;
  StructuredFlightServices services_;
  std::shared_ptr<ControlResponses> control_;
  std::shared_ptr<AsyncStream<Json>> outbound_;
  std::shared_ptr<RequestProducer> producer_;
  std::shared_ptr<InputReader> reader_;
  ControlAdapterServices adapters_;
  std::function<void(const Json&)> onControlRequestSent_;
  std::function<void(const std::string&)> onControlRequestResolved_;
  std::function<event_loop::Deferred<event_loop::Unit>(const Json&)>
    unexpectedResponseCallback_;
};
}
