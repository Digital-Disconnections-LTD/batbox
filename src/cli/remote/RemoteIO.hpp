#pragma once
#include "cli/structured/StructuredFlight.hpp"
#include "event_loop/Loop.hpp"
#include <deque>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

namespace batbox::cli::remote {
using event_loop::Deferred;
using event_loop::Unit;
class CcrInitError:public std::runtime_error {
public:
  CcrInitError(std::string reason,std::string message):
    std::runtime_error(std::move(message)),reason_(std::move(reason)){}
  const std::string& reason()const{return reason_;}
private:
  std::string reason_;
};
struct Transport {
  bool isSse=false;
  std::function<void(std::function<void(const std::string&)>)> setOnData;
  std::function<void(std::function<void()>)> setOnClose;
  std::function<Deferred<Unit>()> connect;
  std::function<Deferred<Unit>(const Json&)> write;
  std::function<void()> close;
};
struct CcrClient {
  std::function<Deferred<Json>()> initialize;
  std::function<Deferred<Unit>(const Json&)> writeEvent;
  std::function<Deferred<Unit>()> flushInternalEvents;
  std::function<std::size_t()> internalEventsPending;
  std::function<Deferred<Unit>()> close;
  std::function<Deferred<Json>(const std::string&,const Json&,const Json&)>
    writeInternalEvent;
  std::function<Deferred<Json>()> readInternalEvents,readSubagentInternalEvents;
  std::function<void(const std::string&,const std::string&)> reportDelivery;
  std::function<void(const std::string&,const Json&)> reportState;
  std::function<void(const Json&)> reportMetadata;
};
struct Services {
  structured::StructuredFlightServices structured;
  std::function<std::optional<std::string>()> getFlightIngressAuthToken;
  std::function<std::string()> getFlightId;
  std::function<std::optional<std::string>(const std::string&)> environment;
  std::function<bool()> isDebugMode;
  std::function<double()> keepAliveIntervalMs;
  std::function<std::shared_ptr<Transport>(const Json&,const Json&,
    const std::string&,std::function<Json()>)> getTransportForUrl;
  std::function<std::shared_ptr<CcrClient>(Transport&,const Json&)> createCcrClient;
  std::function<void(std::function<Deferred<Unit>()>)> registerCleanup;
  std::function<void(std::function<Deferred<Json>(
    const std::string&,const Json&,const Json&)>)> setInternalEventWriter;
  std::function<void(std::function<Deferred<Json>()>,
    std::function<Deferred<Json>()>)> setInternalEventReaders;
  std::function<void(std::function<void(const std::string&,
    const std::string&)>)> setCommandLifecycleListener;
  std::function<void(std::function<void(const std::string&,const Json&)>)>
    setFlightStateChangedListener;
  std::function<void(std::function<void(const Json&)>)>
    setFlightMetadataChangedListener;
  std::function<void(const std::string&,const Json&)> logForDebugging;
  std::function<void(const std::string&)> logError;
  std::function<void(int,const std::string&)> gracefulShutdown;
  std::function<void(const std::string&)> writeToStdout;
  std::function<std::string(const Json&)> ndjsonSafeStringify;
  std::function<Deferred<std::optional<std::string>>()> nextInitialPoetryChunk;
  std::string environmentRunnerVersionKey="BATBOX_ENVIRONMENT_RUNNER_VERSION";
  std::string environmentKindKey="BATBOX_ENVIRONMENT_KIND";
  std::string ccrV2Key="BATBOX_USE_CCR_V2";
};

class RemoteIO {
public:
  RemoteIO(event_loop::Loop& loop,const std::string& streamUrl,
    Services services,bool replayUserMessages=false);
  ~RemoteIO();
  structured::StructuredFlight& structured(){return *structured_;}
  const structured::StructuredFlight& structured()const{return *structured_;}
  Deferred<Unit> write(const Json& message);
  Deferred<Unit> flushInternalEvents();
  std::size_t internalEventsPending()const;
  Deferred<Json> restoredWorkerState()const{return restoredWorkerState_;}
  void close();
private:
  struct Input {
    explicit Input(event_loop::Loop& owner):loop(owner){}
    event_loop::Loop& loop;
    std::deque<std::string> chunks;
    std::optional<Deferred<std::optional<std::string>>> waiting;
    bool ended=false;
    Deferred<std::optional<std::string>> next();
    void write(std::string data);
    void end();
  };
  void beginInitialPoetry();
  void scheduleKeepAlive(double intervalMs);
  Json refreshedHeaders()const;
  event_loop::Loop& loop_;
  Services services_;
  std::shared_ptr<Input> input_;
  std::unique_ptr<structured::StructuredFlight> structured_;
  std::shared_ptr<Transport> transport_;
  std::shared_ptr<CcrClient> ccr_;
  Deferred<Json> restoredWorkerState_;
  event_loop::Loop::TimerHandle keepAliveTimer_;
  std::shared_ptr<bool> alive_=std::make_shared<bool>(true);
  bool bridge_=false,debug_=false,closed_=false;
};
}
