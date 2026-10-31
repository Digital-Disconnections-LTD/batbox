#pragma once
#include "event_loop/Deferred.hpp"
#include "mcp/Mcp.hpp"
#include <map>
#include <optional>
#include <set>

namespace batbox::mcp::manager {
struct ServerResult {
  std::string name,type="pending",error;
  Json config=Json::object(),commands=Json::array(),resources=Json::array();
  std::shared_ptr<Connection> connection;
  std::vector<gadgets::Gadget> gadgets;
  std::shared_ptr<void> owner;
  std::function<void()> close;
  std::size_t reconnectAttempt=0,maxReconnectAttempts=0;
};
struct Update {
  ServerResult client;
  std::optional<std::vector<gadgets::Gadget>> gadgets;
  std::optional<Json> commands,resources;
};
struct Inputs {
  event_loop::Loop& loop;
  Json configurations=Json::object();
  std::function<event_loop::Deferred<ServerResult>(const std::string&,const Json&,ClientHandlers)> connect;
  std::function<event_loop::Deferred<ServerResult>(const ServerResult&,const std::string&)> refresh;
  std::function<bool(const std::string&)> isDisabled;
  std::function<void(const std::string&,bool)> setEnabled;
  std::function<void(const std::vector<ServerResult>&)> changed;
  std::function<void(const std::string&,const std::string&)> diagnostic;
  std::function<Json(const std::string&,const Json&)> serverRequest;
  std::function<event_loop::Deferred<Json>(const std::string&,const Json&,
    std::stop_token)>
    serverRequestAsync;
  std::function<void(const std::string&,const Json&)> notification;
  std::function<void()> cancel;
  explicit Inputs(event_loop::Loop& owner):loop(owner){}
};
class Manager:public std::enable_shared_from_this<Manager> {
public:
  static std::shared_ptr<Manager> create(Inputs inputs);
  ~Manager();
  event_loop::Deferred<event_loop::Unit> start();
  event_loop::Deferred<ServerResult> reconnect(const std::string& name);
  event_loop::Deferred<event_loop::Unit> toggle(const std::string& name);
  event_loop::Deferred<event_loop::Unit> configure(Json configurations);
  event_loop::Deferred<Json> invoke(const std::string& name,const std::string& method,
    const Json& params=Json::object(),std::stop_token signal={});
  event_loop::Deferred<event_loop::Unit> shutdown();
  Json clients()const;
  Json resources()const;
  std::vector<ServerResult> snapshot()const;
  std::vector<gadgets::Gadget> gadgets()const;
  // SDK management controls acknowledge only after the queued snapshot has
  // reached the roster/AppState changed callback.
  void flushPending();
  void updateServer(Update update);
private:
  explicit Manager(Inputs inputs);
  Inputs inputs_;
  bool started_=false,closing_=false;
  std::vector<ServerResult> servers_;
  std::vector<Update> pending_;
  event_loop::Loop::TimerHandle flushTimer_;
  std::map<std::string,event_loop::Loop::TimerHandle> reconnectTimers_;
  std::map<std::string,std::uint64_t> generations_;
  std::size_t activeJobs_=0;
  std::optional<event_loop::Deferred<event_loop::Unit>> shutdownResult_;
  ServerResult& server(const std::string& name);
  void requireOwner()const;
  void flush();
  ClientHandlers handlers(const std::string& name,std::uint64_t generation);
  void onConnectionAttempt(ServerResult result,std::uint64_t generation);
  void onClosed(const std::string& name,std::uint64_t generation);
  void automaticReconnect(const std::string& name,std::uint64_t generation,std::size_t attempt);
  void changed(const std::string& name,std::uint64_t generation,const Json& notification);
  void cancelReconnect(const std::string& name);
  void dispose(ServerResult result);
  void beginJob();
  void endJob();
};
} // namespace batbox::mcp::manager
