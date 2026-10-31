#pragma once
#include "client_factory/oauth_config/OAuthConfig.hpp"
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace batbox::auth::oauth_listener {
struct Response {
  virtual ~Response()=default;
  virtual void writeHead(int status,const Json& headers)=0;
  virtual void end(const std::string& body="")=0;
};
struct Request {
  std::string url,host;
  std::shared_ptr<Response> response;
};
struct Server {
  virtual ~Server()=default;
  virtual event_loop::Deferred<int> start(
    const std::optional<int>& port)=0;
  virtual void onRequest(std::function<void(const Request&)> handler)=0;
  virtual void onError(std::function<void(const std::string&)> handler)=0;
  virtual void removeAllListeners()=0;
  virtual void close()=0;
};
struct Services {
  std::function<std::shared_ptr<Server>()> createServer;
  std::function<client_factory::oauth_config::Config()> getConfig;
  std::function<void(const std::string&)> logError;
  std::string inferenceScope;
};
class AuthCodeListener {
public:
  AuthCodeListener(event_loop::Loop& loop,Services services,
    std::string callbackPath="/callback");
  ~AuthCodeListener();
  event_loop::Deferred<int> start(std::optional<int> port=std::nullopt);
  int getPort()const;
  bool hasPendingResponse()const;
  event_loop::Deferred<std::string> waitForAuthorization(
    const std::string& state,
    const std::function<event_loop::Deferred<event_loop::Unit>()>& onReady);
  void handleSuccessRedirect(const std::vector<std::string>& scopes,
    const std::function<void(Response&,const std::vector<std::string>&)>&
      customHandler={});
  void handleErrorRedirect();
  void close();
private:
  event_loop::Loop& loop_;
  Services services_;
  std::shared_ptr<Server> server_;
  std::shared_ptr<int> lifetime_=std::make_shared<int>(0);
  std::string callbackPath_;
  int port_=0;
  std::optional<event_loop::Deferred<std::string>> pendingPromise_;
  std::optional<std::string> expectedState_;
  std::shared_ptr<Response> pendingResponse_;
  void handleRedirect(const Request& request);
  void validateAndRespond(const std::optional<std::string>& code,
    const std::optional<std::string>& state,
    std::shared_ptr<Response> response);
  void handleError(const std::string& message);
  void resolve(const std::string& code);
  void reject(const std::string& message);
};
}
