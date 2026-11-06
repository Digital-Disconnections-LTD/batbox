#include "providers/gcp/VertexClient.hpp"
#include "runtime/Version.hpp"
#include "providers/cloud/ErrorShape.hpp"
#include "transport/Errors.hpp"
#include "transport/Messages.hpp"
#include "transport/SSE.hpp"
#include <cstdlib>
#include <stdexcept>

namespace batbox::providers::gcp {
namespace {
std::string firstEnv(std::initializer_list<const char*> names) {
  for (const auto* name : names)
    if (const char* value = std::getenv(name); value && *value) return value;
  return {};
}
std::string encodeModel(std::string_view model) {
  static constexpr char digits[] = "0123456789ABCDEF";
  std::string out;
  for (const unsigned char c : model) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~' || c == '@')
      out += static_cast<char>(c);
    else { out += '%'; out += digits[c >> 4]; out += digits[c & 15]; }
  }
  return out;
}
transport::MessagesProtocol vertexProtocol() {
  transport::MessagesProtocol protocol;
  protocol.betaHeader = "anthropic-beta";
  return protocol;
}
void dispatch(const transport::ServerSentEvent& event, const Json& headers, const transport::OnEvent& onEvent) {
  if (event.event) { transport::dispatchSSE(event,headers,onEvent); return; }
  // Events without an event: line still carry their type in the JSON.
  if (event.data.empty()) return;
  auto json = value::parse(event.data,false);
  if (json.is_discarded() || !json.is_object() || !json.contains("type")) return;
  if (json["type"] == "ping") return;
  if (json["type"] == "error") throw transport::APIError(std::nullopt,json,headers,event.data);
  onEvent(json);
}
} // namespace
const std::vector<std::string>& vertexAnthropicModels() {
  static const std::vector<std::string> models{
    "claude-opus-5-5","claude-sonnet-5-5","claude-haiku-5-5","claude-fable-5-1","claude-opus-5","claude-sonnet-5",
    "claude-fable-5","claude-opus-4-8","claude-opus-4-7","claude-opus-4-6","claude-sonnet-4-6","claude-opus-4-5",
    "claude-sonnet-4-5","claude-haiku-4-5"};
  return models;
}
std::string vertexProject(const VertexConfig& config, const GoogleTokenSource* tokens) {
  if (!config.project.empty()) return config.project;
  if (auto project = firstEnv({"GOOGLE_VERTEX_PROJECT","GOOGLE_CLOUD_PROJECT","GCP_PROJECT","GCLOUD_PROJECT"}); !project.empty()) return project;
  return tokens ? tokens->credentialsProject() : std::string();
}
std::string vertexRegion(const VertexConfig& config) {
  if (!config.region.empty()) return config.region;
  if (auto region = firstEnv({"GOOGLE_VERTEX_LOCATION","GOOGLE_CLOUD_LOCATION","VERTEX_LOCATION"}); !region.empty()) return region;
  return "global";
}
std::string vertexHost(const std::string& region) {
  return region == "global" ? "https://aiplatform.googleapis.com" : "https://"+region+"-aiplatform.googleapis.com";
}
std::string vertexModelUrl(const VertexConfig& config, const std::string& project, const std::string& region,
                           const std::string& model, bool stream) {
  const auto base = config.baseUrl.empty() ? vertexHost(region) : config.baseUrl;
  return cloud::joinUrl(base,"/v1/projects/"+cloud::percentEncodeComponent(project)+"/locations/"+cloud::percentEncodeComponent(region)+
                             "/publishers/anthropic/models/"+encodeModel(model)+(stream ? ":streamRawPredict" : ":rawPredict"));
}
Json vertexBody(const Json& anthropicBody, bool stream) {
  auto body = transport::prepareMessageRequest(anthropicBody,Json::object(),vertexProtocol()).body;
  body.erase("model");
  if (!body.contains("anthropic_version")) body["anthropic_version"] = vertexAnthropicVersion;
  body["stream"] = stream;
  return body;
}
cloud::HttpRequest prepareVertexRequest(const VertexConfig& config, const std::string& project, const std::string& region,
                                        const std::string& accessToken, const Json& anthropicBody, bool stream) {
  const auto& model = value::property(anthropicBody,"model");
  if (!model.is_string() || model.get<std::string>().empty()) throw std::invalid_argument("Vertex AI requests need a model id");
  if (project.empty())
    throw std::invalid_argument("Vertex AI needs a project: set project on the endpoint or GOOGLE_VERTEX_PROJECT / GOOGLE_CLOUD_PROJECT");
  auto headers = transport::prepareMessageRequest(anthropicBody,config.headers,vertexProtocol()).headers;
  cloud::HttpRequest request;
  request.stream = stream;
  request.url = vertexModelUrl(config,project,region,model.get<std::string>(),stream);
  request.body = value::stringify(vertexBody(anthropicBody,stream));
  if (!headers.contains("content-type")) headers["content-type"] = "application/json";
  if (!headers.contains("accept")) headers["accept"] = stream ? "text/event-stream" : "application/json";
  if (!headers.contains("user-agent")) headers["user-agent"] = runtime::userAgent();
  headers["authorization"] = "Bearer "+accessToken;
  for (auto it = headers.begin(); it != headers.end(); ++it)
    if (!it.value().is_null()) request.headers.emplace_back(it.key(),it.value().get<std::string>());
  return request;
}
VertexClient::VertexClient(VertexConfig settings, GoogleAuthOptions authOptions)
  : config(std::move(settings)),
    tokens([&] {
      authOptions.network = config.network;
      return std::make_shared<GoogleTokenSource>(std::move(authOptions));
    }()),
    http(config.network) {}
VertexClient::VertexClient(VertexConfig settings, std::shared_ptr<GoogleTokenSource> source)
  : config(std::move(settings)), tokens(std::move(source)), http(config.network) {
  if (!tokens) throw std::invalid_argument("VertexClient needs a token source");
}
transport::Response VertexClient::request(const Json& anthropicBody, bool stream, const transport::OnEvent& onEvent, std::stop_token stop,
                                          const transport::OnResponse& onResponse, transport::Timeout timeout) {
  std::lock_guard lock(mutex);
  if (stop.stop_requested()) throw transport::APIUserAbortError();
  if (stream && !onEvent) throw std::invalid_argument("Streaming requests require an event consumer");
  const auto project = vertexProject(config,tokens.get());
  const auto region = vertexRegion(config);
  for (int attempt = 0;; ++attempt) {
    std::string accessToken = config.bearerToken;
    if (accessToken.empty()) {
      try { accessToken = tokens->token().token; }
      catch (const std::exception& error) { cloud::throwMapped({401,"authentication_error",error.what()},Json::object()); }
    }
    cloud::HttpRequest prepared;
    try { prepared = prepareVertexRequest(config,project,region,accessToken,anthropicBody,stream); }
    catch (const std::invalid_argument& error) { cloud::throwMapped({400,"invalid_request_error",error.what()},Json::object()); }
    prepared.timeout = timeout;
    transport::Response response;
    transport::SSEDecoder decoder;
    const auto onHeaders = [&](long status, const Json& headers) {
      response.status = status;
      response.headers = headers;
      if (status >= 200 && status < 300 && onResponse) onResponse(response);
    };
    const auto onBytes = [&](std::string_view bytes) {
      for (const auto& event : decoder.decode(bytes)) dispatch(event,response.headers,onEvent);
    };
    auto result = http.perform(prepared,stop,onHeaders,stream ? cloud::OnBytes(onBytes) : cloud::OnBytes());
    response.status = result.status;
    response.headers = result.headers;
    if (result.status < 200 || result.status >= 300) {
      if (result.status == 401 && attempt == 0 && config.bearerToken.empty()) {
        tokens->invalidate();
        continue;
      }
      cloud::throwGoogleError(result.status,result.body,result.headers);
    }
    if (stream) {
      for (const auto& event : decoder.flush()) dispatch(event,response.headers,onEvent);
    } else if (!result.body.empty()) {
      response.body = value::parse(result.body);
    }
    return response;
  }
}
Json VertexClient::listModels() {
  Json data = Json::array();
  for (const auto& id : config.models.empty() ? vertexAnthropicModels() : config.models) data.push_back(Json{{"id",id}});
  return Json{{"data",std::move(data)}};
}
} // namespace batbox::providers::gcp
