#pragma once
#include "cli/remote/RemoteIO.hpp"
#include "event_loop/Loop.hpp"
#include <deque>
#include <memory>
#include <optional>
#include <string>

namespace batbox::cli::remote {
struct WebSocketConnection {
  std::function<void(const std::string&)> send;
  std::function<void()> ping,close,removeListeners;
};
struct WebSocketCallbacks {
  std::function<void(const std::optional<std::string>&)> onOpen;
  std::function<void(const std::string&)> onMessage;
  std::function<void(const std::string&)> onError;
  std::function<void(int)> onClose;
  std::function<void()> onPong;
};
struct WebSocketServices {
  std::function<std::shared_ptr<WebSocketConnection>(
    const std::string&,const Json&,WebSocketCallbacks)> open;
  std::function<double()> nowMs,randomUnit;
  std::function<void(const std::string&,const Json&)> logDebug;
  std::function<void(std::function<void()>)> registerFlightActivity;
  std::function<void()> unregisterFlightActivity;
  std::function<std::string(const Json&)> jsonStringify;
  bool remoteMode=false;
};
struct WebSocketOptions {
  bool autoReconnect=true,isBridge=false;
};
class WebSocketTransport:public std::enable_shared_from_this<WebSocketTransport> {
public:
  WebSocketTransport(event_loop::Loop& loop,Json url,Json headers,
    std::optional<std::string> flightId,std::function<Json()> refreshHeaders,
    WebSocketServices services,WebSocketOptions options={});
  Deferred<Unit> connect();
  Deferred<Unit> write(const Json& message);
  void close();
  void setOnData(std::function<void(const std::string&)> callback);
  void setOnClose(std::function<void(int)> callback);
  void setOnConnect(std::function<void()> callback);
  bool isConnectedStatus()const;
  bool isClosedStatus()const;
  std::string getStateLabel()const;
  std::shared_ptr<Transport> asTransport();
private:
  void opened(const std::optional<std::string>& lastId);
  void received(const std::string& data);
  void connectionError(std::optional<int> code={});
  void doDisconnect();
  bool sendLine(const std::string& line);
  void replayBufferedMessages(const std::string& lastId);
  void schedulePing();
  void scheduleKeepAlive();
  void stopTimers();
  enum class State {Idle,Connected,Reconnecting,Closing,Closed};
  event_loop::Loop& loop_;
  Json url_,headers_;
  std::optional<std::string> flightId_,lastSentId_;
  std::function<Json()> refreshHeaders_;
  WebSocketServices services_;
  WebSocketOptions options_;
  std::shared_ptr<WebSocketConnection> socket_;
  std::function<void(const std::string&)> onData_;
  std::function<void(int)> onClose_;
  std::function<void()> onConnect_;
  std::deque<Json> messageBuffer_;
  State state_=State::Idle;
  int reconnectAttempts_=0;
  std::optional<double> reconnectStartTime_,lastReconnectAttemptTime_;
  double lastActivityTime_=0,connectStartTime_=0,lastPingTick_=0;
  bool pongReceived_=true;
  event_loop::Loop::TimerHandle reconnectTimer_,pingTimer_,keepAliveTimer_;
};
std::shared_ptr<Transport> makeWebSocketTransport(
  event_loop::Loop& loop,const Json& url,const Json& headers,
  const std::string& flightId,std::function<Json()> refreshHeaders,
  WebSocketServices services,WebSocketOptions options={});
}
