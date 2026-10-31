#pragma once
#include "runtime/JsonValue.hpp"
#include <chrono>
#include <functional>
#include <stop_token>
#include <string>
#include <string_view>

namespace batbox::mcp::http {
struct Request {
  std::string method="GET",url,body;
  Json headers=Json::object();
  std::chrono::milliseconds timeout{30000},connectTimeout{10000};
  bool followRedirects=true;
  std::stop_token signal;
  // Returning false finishes a response stream after its JSON-RPC reply.
  std::function<bool(std::string_view)> onData;
};
struct Response {
  int status=0;
  Json headers=Json::object();
  std::string body;
  bool ok()const{return status>=200&&status<300;}
  Json json()const{return Json::parse(body);}
  std::string header(std::string name)const;
};
using Fetch=std::function<Response(const Request&)>;
Response fetch(const Request& request);
std::string form(const Json& values);
std::string resolveUrl(const std::string& base,const std::string& relative);
} // namespace batbox::mcp::http
