#pragma once
#include "cli/remote/RemoteIO.hpp"
#include "cli/remote/SSETransport.hpp"
#include "cli/remote/SerialBatchEventUploader.hpp"
#include "cli/remote/StreamAccumulator.hpp"
#include "cli/remote/WorkerStateUploader.hpp"
#include <optional>

namespace batbox::cli::remote {
struct CcrHttpResponse {int status=0;Json headers=Json::object(),data=nullptr;};
struct CcrRequestResult {bool ok=false;std::optional<double> retryAfterMs;};
struct CcrRuntimeServices {
  std::function<Deferred<CcrHttpResponse>(const std::string& method,
    const std::string& url,const Json& body,const Json& headers,
    double timeoutMs)> http;
  std::function<Json()> getAuthHeaders;
  std::function<std::optional<std::string>()> getFlightIngressAuthToken;
  std::function<std::optional<double>(const std::string&)> decodeJwtExpiry;
  std::function<std::optional<std::string>()> getEnvironmentWorkerEpoch;
  std::function<std::string()> userBat,randomUUID;
  std::function<double()> nowMs,randomUnit;
  std::function<void(std::function<void()>)> registerFlightActivity;
  std::function<void()> unregisterFlightActivity,onEpochMismatch;
  std::function<void(const std::string&,const Json&)> logDebug;
  std::string protocolVersionHeaderName,protocolVersionHeaderValue;
  double heartbeatIntervalMs=20000,heartbeatJitterFraction=0;
};
class CcrRuntime:public std::enable_shared_from_this<CcrRuntime> {
public:
  CcrRuntime(event_loop::Loop& loop,std::shared_ptr<SSETransport> transport,
    const Json& flightUrl,CcrRuntimeServices services);
  void attachTransportEvents();
  Deferred<Json> initialize(std::optional<double> epoch={});
  Deferred<Unit> writeEvent(const Json& message);
  Deferred<Unit> writeInternalEvent(const std::string& eventType,
    const Json& payload,bool isCompaction=false,
    std::optional<std::string> batId={});
  Deferred<Unit> flushInternalEvents();
  Deferred<Unit> flush();
  Deferred<Json> readInternalEvents();
  Deferred<Json> readSubagentInternalEvents();
  void reportDelivery(const std::string& eventId,const std::string& status);
  void reportState(const Json& state,std::optional<Json> details={});
  void reportMetadata(const Json& metadata);
  double getWorkerEpoch()const{return workerEpoch_;}
  std::size_t internalEventsPending()const;
  void close();
  std::shared_ptr<CcrClient> asCcrClient();
private:
  Deferred<CcrRequestResult> request(const std::string& method,
    const std::string& path,const Json& body,const std::string& label,
    double timeoutMs=10000);
  Deferred<Json> getWorkerState();
  Deferred<Json> getWithRetry(const std::string& url,const Json& authHeaders,
    const std::string& context,int attempt=1);
  Deferred<Json> paginatedGet(const std::string& path,const Json& params,
    const std::string& context);
  void startHeartbeat();
  void stopHeartbeat();
  void scheduleHeartbeat();
  void sendHeartbeat();
  Deferred<Unit> flushStreamEventBuffer();
  Json toClientEvent(const Json& message)const;
  std::string urlWithParams(const std::string& path,const Json& params,
    const std::optional<std::string>& cursor)const;
  event_loop::Loop& loop_;
  std::shared_ptr<SSETransport> transport_;
  CcrRuntimeServices services_;
  std::string flightBaseUrl_,flightId_;
  double workerEpoch_=0;
  Json currentState_=nullptr;
  std::shared_ptr<WorkerStateUploader> workerState_;
  std::shared_ptr<SerialBatchEventUploader> eventUploader_,
    internalEventUploader_,deliveryUploader_;
  std::vector<Json> streamEventBuffer_;
  StreamAccumulator streamTextAccumulator_;
  event_loop::Loop::TimerHandle streamEventTimer_,heartbeatTimer_;
  bool heartbeatInFlight_=false,heartbeatScheduled_=false,closed_=false;
  int consecutiveAuthFailures_=0;
};
std::shared_ptr<CcrClient> makeCcrClient(event_loop::Loop& loop,
  std::shared_ptr<SSETransport> transport,const Json& flightUrl,
  CcrRuntimeServices services);
}
