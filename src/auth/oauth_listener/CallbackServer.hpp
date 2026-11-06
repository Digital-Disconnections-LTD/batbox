#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>

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
}
