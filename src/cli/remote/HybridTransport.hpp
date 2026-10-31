#pragma once
#include "cli/remote/SerialBatchEventUploader.hpp"
#include "cli/remote/SSETransport.hpp"
#include "cli/remote/WebSocketTransport.hpp"

namespace batbox::cli::remote {
struct HybridServices {
  WebSocketServices websocket;
  std::function<std::optional<std::string>()> getFlightIngressAuthToken;
  std::function<Deferred<SseHttpResponse>(
    const std::string&,const Json&,const Json&)> post;
  std::function<void(const std::string&,const Json&)> logDebug;
  std::function<double()> randomUnit;
  std::function<std::string(const Json&)> jsonStringify;
  std::optional<std::size_t> maxConsecutiveFailures;
  std::function<void(std::size_t,std::size_t)> onBatchDropped;
};
class HybridTransport:public std::enable_shared_from_this<HybridTransport> {
public:
  HybridTransport(event_loop::Loop& loop,Json url,Json headers,
    std::optional<std::string> flightId,std::function<Json()> refreshHeaders,
    HybridServices services,WebSocketOptions options={});
  Deferred<Unit> connect();
  Deferred<Unit> write(const Json& message);
  Deferred<Unit> writeBatch(std::vector<Json> messages);
  Deferred<Unit> flush();
  std::size_t droppedBatchCount()const;
  void close();
  void setOnData(std::function<void(const std::string&)> callback);
  void setOnClose(std::function<void(int)> callback);
  bool isConnectedStatus()const;
  bool isClosedStatus()const;
  std::shared_ptr<Transport> asTransport();
private:
  std::vector<Json> takeStreamEvents();
  void flushStreamEvents();
  Deferred<Unit> postOnce(const std::vector<Json>& events);
  static std::string postUrl(const Json& url);
  event_loop::Loop& loop_;
  std::shared_ptr<WebSocketTransport> websocket_;
  std::shared_ptr<SerialBatchEventUploader> uploader_;
  HybridServices services_;
  std::string postUrl_;
  std::vector<Json> streamEventBuffer_;
  event_loop::Loop::TimerHandle streamEventTimer_,graceTimer_;
  bool closed_=false;
};
std::shared_ptr<Transport> makeHybridTransport(event_loop::Loop& loop,
  const Json& url,const Json& headers,const std::string& flightId,
  std::function<Json()> refreshHeaders,HybridServices services,
  WebSocketOptions options={});
}
