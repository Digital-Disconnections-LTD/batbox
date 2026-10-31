#pragma once
#include "mcp/manager/Manager.hpp"
#include "mcp/auth/Flow.hpp"
#include "mcp/auth/IndigenousServices.hpp"
#include <filesystem>

namespace batbox::mcp::manager {
struct IndigenousInputs {
  event_loop::Loop& owner;
  std::filesystem::path configHome,projectRoot;
  Json configurations=Json::object();
  bool explicitConfigurations=false;
  Services gadgetServices;
  std::function<void(const std::vector<ServerResult>&)> changed;
  std::function<void(const std::string&,const std::string&)> diagnostic;
  std::function<Json(const std::string&,const Json&)> serverRequest;
  // Owner-thread callback for MCP elicitation/create. The transport reader
  // remains live while this Promise waits for an interactive answer.
  std::function<event_loop::Deferred<Json>(const std::string&,const Json&,
    std::stop_token)>
    elicitationRequest;
  std::function<void(const std::string&,const Json&)> notification;
  std::function<void(const std::string&,const std::string&)> authorizationUrl;
  std::function<void(const std::string&,std::function<void(const std::string&)>)> waitingForCallback;
  http::Fetch fetch=http::fetch;
  explicit IndigenousInputs(event_loop::Loop& loop):owner(loop){}
};
// Indigenous SDK/process services for the connection manager. Configuration,
// credentials and enable/disable state live outside the repository.
class IndigenousRuntime:public std::enable_shared_from_this<IndigenousRuntime> {
public:
  static std::shared_ptr<IndigenousRuntime> create(IndigenousInputs inputs);
  std::shared_ptr<Manager> manager()const{return manager_;}
  event_loop::Deferred<event_loop::Unit> start();
  event_loop::Deferred<event_loop::Unit> shutdown();
  event_loop::Deferred<event_loop::Unit> authenticate(const std::string& name,
    std::stop_token signal={},bool skipBrowserOpen=false);
  event_loop::Deferred<event_loop::Unit> logout(const std::string& name);
  event_loop::Deferred<event_loop::Unit> reload(std::filesystem::path projectRoot);
  event_loop::Deferred<ServerResult> connectBatServer(const std::string& name,
    const Json& config);
  void setGadgetServices(Services services);
  const Json& configurations()const{return configurations_;}
private:
  explicit IndigenousRuntime(IndigenousInputs inputs);
  IndigenousInputs inputs_;
  auth::IndigenousServices auth_;
  std::shared_ptr<Manager> manager_;
  Json configurations_,disabled_=Json::array();
  std::stop_source lifetime_;
  std::size_t authJobs_=0;
  std::optional<event_loop::Deferred<event_loop::Unit>> stopped_;
  bool managerStopped_=false;
  void initialize();
  void loadConfiguration();
  void writeDisabled();
  void finishShutdown();
  event_loop::Deferred<ServerResult> connect(const std::string&,const Json&,ClientHandlers);
  event_loop::Deferred<ServerResult> refresh(const ServerResult&,const std::string&);
  ServerResult discover(ServerResult result,const std::string& kind,std::stop_token signal,
    const Services& gadgetServices);
  auth::Services authenticationServices(const std::string& name);
  event_loop::Deferred<Json> requestElicitation(const std::string& name,
    const Json& request,std::stop_token signal);
};
Json expandServerConfiguration(const Json& config);
Json loadMcpConfigurations(const std::filesystem::path& privatePath,
  const std::filesystem::path& projectRoot);
} // namespace batbox::mcp::manager
