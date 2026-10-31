#pragma once
#include "mcp/Mcp.hpp"
#include "mcp/auth/Provider.hpp"
#include "mcp/http/Http.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <thread>

namespace batbox::mcp {
struct RemoteConfig {
  std::string name,url,type="http",originalCwd;
  Json headers=Json::object();
  http::Fetch fetch=http::fetch;
  http::Fetch cleanupFetch=http::fetch;
  std::shared_ptr<auth::Provider> auth;
  std::chrono::milliseconds connectTimeout{30000},requestTimeout{60000},gadgetTimeout{100000000};
  ClientHandlers handlers;
  std::stop_token lifecycleSignal;
};
class RemoteClient:public std::enable_shared_from_this<RemoteClient> {
public:
  explicit RemoteClient(RemoteConfig config);
  ~RemoteClient();
  void initialize(std::stop_token signal={});
  void shutdown()noexcept;
  void ensureConnected();
  void expireFlight();
  Json invoke(const std::string& method,const Json& params=Json::object(),
    std::stop_token signal={},const std::function<void(const Json&)>& progress={});
  Json listGadgets(std::stop_token signal={});
  GadgetCallResult callGadget(const std::string& name,const Json& args,
    std::stop_token signal={},const std::function<void(const Json&)>& progress={});
  Json capabilities()const;
  Json serverInfo()const;
  std::string instructions()const;
  bool connected()const{return connected_;}
private:
  struct Pending {
    std::mutex mutex;
    std::condition_variable ready;
    Json result;
    std::exception_ptr error;
    bool complete=false;
    std::string method,gadget;
    std::function<void(const Json&)> progress;
  };
  RemoteConfig config_;
  std::stop_source requestLifetime_;
  std::atomic<bool> connected_{false},closing_{false},expired_{false};
  std::atomic<std::int64_t> nextId_{0};
  mutable std::mutex stateMutex_,pendingMutex_;
  std::mutex initializationMutex_;
  std::condition_variable endpointReady_;
  std::map<std::int64_t,std::shared_ptr<Pending>> pending_;
  std::string endpoint_,flightId_,protocol_,instructions_,lastEventId_;
  Json capabilities_=Json::object(),serverInfo_=Json::object();
  std::jthread listener_;
  Json headers(const std::string& accept);
  Json request(const std::string& method,const Json& params,std::stop_token signal,
    const std::function<void(const Json&)>& progress,std::chrono::milliseconds timeout);
  void send(const Json& message,std::stop_token signal,std::chrono::milliseconds timeout,bool retryAuth=true);
  void receive(const Json& message);
  void listen(std::stop_token signal);
  void failAll(std::exception_ptr error);
  void closeNotification();
};
std::shared_ptr<Connection> makeRemoteConnection(const std::shared_ptr<RemoteClient>& client,
  const std::string& name,const std::string& transport,bool skipPrefix=false);
} // namespace batbox::mcp
