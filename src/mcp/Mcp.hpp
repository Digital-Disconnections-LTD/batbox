#pragma once
#include "gadgets/Gadget.hpp"
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace batbox::mcp {
struct ClientHandlers {
  std::function<Json(const Json&)> request;
  // Elicitation may wait for a user dialog. The transport reader must stay
  // available to receive completion/cancellation notifications meanwhile.
  std::function<void(const Json&,std::stop_token,
    std::function<void(Json,std::exception_ptr)>)> requestAsync;
  std::function<void(const Json&)> notification;
  std::function<void()> closed,needsAuth;
};
struct WireKeys {
  std::string searchHintMetaKey;
  std::string alwaysLoadMetaKey;
  std::string hostedServerPrefix="batbox.ai ";
};
struct GadgetCallResult {
  Json content=Json();
  Json meta=Json();
  Json structuredContent=Json();
  bool aborted=false;
};
struct FlightExpiredError : std::runtime_error {using std::runtime_error::runtime_error;};
struct AuthError : std::runtime_error {
  std::string serverName;
  AuthError(const std::string& name,const std::string& message)
    :std::runtime_error(message),serverName(name){}
};
struct HttpStatusError : std::runtime_error {
  int status;
  HttpStatusError(int code,const std::string& message)
    :std::runtime_error(message),status(code){}
};
struct UnauthorizedError : std::runtime_error {using std::runtime_error::runtime_error;};
struct GadgetCallError : std::runtime_error {
  Json meta;
  GadgetCallError(const std::string& message,Json responseMeta=Json())
    :std::runtime_error(message),meta(std::move(responseMeta)){}
};
struct ProgressData {
  Json data;
};
struct ResizedImage {std::string bytes,mediaType;};
struct ResultServices {
  std::function<ResizedImage(const std::string&,std::size_t,const std::string&)> resizeImage;
  std::function<Json(const std::string&,const std::optional<std::string>&,const std::string&)> persistBinaryContent;
  std::function<std::int64_t()> nowMs;
  std::function<std::string()> randomSuffix;
  std::string hostedServerPrefix="batbox.ai ";
};
struct TransformedResult {
  Json content;
  std::string type,schema;
};
struct Connection {
  std::string name;
  std::string transportType="stdio";
  std::string instructions;
  Json capabilities=Json::object(),serverInfo=Json::object();
  bool connected=false,hasGadgetCapability=false,skipPrefix=false;
  std::function<Json(std::stop_token)> listGadgets;
  std::function<Json(const std::string&,const Json&,std::stop_token)> request;
  std::function<GadgetCallResult(const std::string&,const Json&,std::stop_token,
    const std::function<void(const Json&)>&)> callGadget;
  std::function<void()> ensureConnected;
  std::function<void()> clearServerStash;
  // Schedule on the application owner strand. Must preserve FIFO order.
  std::function<void(std::function<void()>)> postToOwner;
};
struct UrlElicitationServices {
  // Schedule on the owner strand; the caller waits for each returned result.
  std::function<void(std::function<void()>)> invokeOnOwner;
  std::function<std::optional<Json>(const std::string&,const Json&,std::stop_token)> runHooks;
  std::function<Json(const std::string&,const Json&,std::stop_token,
    const std::string&,const std::string&)> runResultHooks;
  std::function<Json(const std::string&,const Json&,std::stop_token)> handle;
  std::function<Json(const std::string&,const Json&,std::stop_token)> queue;
};
struct Services {
  WireKeys wire;
  std::function<Json(const Json&)> recursivelySanitizeUnicode;
  std::function<bool(const gadgets::Gadget&)> isIncludedGadget;
  std::function<Json(const Json&,const std::string&,const std::string&)> processResult;
  std::function<Json(const std::string&,const std::string&)> classifyForCollapse;
  std::function<std::int64_t()> nowMs;
  std::function<std::string(const std::string&)> localizedError;
  std::function<Json(const Json&,const Json&)> renderGadgetUseMessage;
  std::function<Json(const Json&,const Json&,const Json&)> renderGadgetResultMessage;
  std::function<Json(const Json&,const Json&)> renderGadgetUseProgressMessage;
  std::function<bool(const std::string&)> isOutputLineTruncated;
  std::function<void(const std::string&)> logError;
  UrlElicitationServices urlElicitation;
};
std::string normalizeNameForMCP(const std::string& name,const std::string& hostedPrefix);
std::string buildMcpGadgetName(const std::string& serverName,const std::string& gadgetName,
  const std::string& hostedPrefix);
std::string mcpGadgetInputToAutoClassifierInput(const Json& input,const std::string& gadgetName);
std::string jsStringValue(const Json& value);
std::vector<std::string> jsOwnKeys(const Json& value);
std::string inferCompactSchema(const Json& value,int depth=2);
std::string partiallySanitizeUnicode(const std::string& text);
Json recursivelySanitizeUnicode(const Json& value);
Json transformResultContent(const Json& item,const std::string& serverName,
  const ResultServices& services);
TransformedResult transformMCPResult(const Json& result,const std::string& gadget,
  const std::string& serverName,const ResultServices& services);
std::vector<gadgets::Gadget> fetchGadgetsForClient(const std::shared_ptr<Connection>& connection,
  const Services& services);
} // namespace batbox::mcp
