#pragma once
#include "cli/remote/RemoteIO.hpp"
#include "cli/remote/SSEFrames.hpp"
#include <memory>
#include <optional>
#include <set>

namespace batbox::cli::remote {
struct StreamClientEvent {
  Json value;
};
struct SseStreamCallbacks {
  std::function<void(int,bool)> onHeaders;
  std::function<void(const std::string&)> onBytes;
  std::function<void(const std::optional<std::string>&)> onEnd;
};
struct SseStreamHandle {std::function<void()> abort;};
struct SseHttpResponse {int status=0;Json data=nullptr;};
struct SseServices {
  std::function<std::shared_ptr<SseStreamHandle>(
    const std::string&,const Json&,SseStreamCallbacks)> openStream;
  std::function<Deferred<SseHttpResponse>(const std::string&,const Json&,const Json&)> post;
  std::function<Json()> getAuthHeaders;
  std::function<std::string()> userBat;
  std::function<double()> nowMs,randomUnit;
  std::function<std::string(const Json&)> jsonStringify;
  std::function<void(const std::string&,const Json&)> logDebug;
  std::string protocolVersionHeaderName,protocolVersionHeaderValue;
};
class SSETransport:public std::enable_shared_from_this<SSETransport> {
public:
  SSETransport(event_loop::Loop& loop,Json url,Json headers,
    std::optional<std::string> flightId,std::function<Json()> refreshHeaders,
    std::optional<double> initialSequenceNum,SseServices services);
  Deferred<Unit> connect();
  Deferred<Unit> write(const Json& message);
  void close();
  void setOnData(std::function<void(const std::string&)> callback);
  void setOnClose(std::function<void(std::optional<int>)> callback);
  void setOnEvent(std::function<void(const StreamClientEvent&)> callback);
  bool isConnectedStatus()const;
  bool isClosedStatus()const;
  std::string getStateLabel()const;
  double getLastSequenceNum()const{return lastSequenceNum_;}
  std::shared_ptr<Transport> asTransport();
private:
  enum class State {Idle,Connected,Reconnecting,Closing,Closed};
  void receivedHeaders(int status,bool hasBody,const Deferred<Unit>& result);
  void receivedBytes(const std::string& bytes);
  void streamEnded(const std::optional<std::string>& error,const Deferred<Unit>& result);
  void processFrame(const SSEFrame& frame);
  void handleSSEFrame(const std::string& eventType,const std::string& data);
  void handleConnectionError();
  void resetLivenessTimer();
  void clearLivenessTimer();
  void postAttempt(Json message,Json headers,int attempt,const Deferred<Unit>& result);
  std::string sseUrl()const;
  static std::string postUrl(const Json& url);
  event_loop::Loop& loop_;
  Json url_,headers_;
  std::optional<std::string> flightId_;
  std::function<Json()> refreshHeaders_;
  SseServices services_;
  State state_=State::Idle;
  std::shared_ptr<SseStreamHandle> stream_;
  std::string byteBuffer_;
  std::set<double> seenSequenceNums_;
  double lastSequenceNum_=0;
  int reconnectAttempts_=0;
  std::optional<double> reconnectStartTime_;
  event_loop::Loop::TimerHandle reconnectTimer_,livenessTimer_;
  std::function<void(const std::string&)> onData_;
  std::function<void(std::optional<int>)> onClose_;
  std::function<void(const StreamClientEvent&)> onEvent_;
  std::string postUrl_;
  std::uint64_t streamEpoch_=0;
  bool decoderStarted_=false;
};
std::shared_ptr<Transport> makeSseTransport(event_loop::Loop& loop,
  const Json& url,const Json& headers,const std::string& flightId,
  std::function<Json()> refreshHeaders,SseServices services,
  std::optional<double> initialSequenceNum={});
}
