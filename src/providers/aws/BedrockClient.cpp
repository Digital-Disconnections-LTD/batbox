#include "providers/aws/BedrockClient.hpp"
#include "runtime/Version.hpp"
#include "providers/aws/SigV4.hpp"
#include "providers/cloud/Crypto.hpp"
#include "providers/cloud/ErrorShape.hpp"
#include "transport/Errors.hpp"
#include "transport/Messages.hpp"
#include <cstdlib>
#include <set>

namespace batbox::providers::aws {
namespace {
std::string env(const char* name) {
  const char* value = std::getenv(name);
  return value ? std::string(value) : std::string();
}
bool authFailure(const Json& body) {
  const auto& error = value::property(body,"error");
  return value::property(error,"type") == "authentication_error";
}
Json withRequestId(Json headers) {
  if (headers.is_object() && !headers.contains("request-id") && headers.contains("x-amzn-requestid"))
    headers["request-id"] = headers["x-amzn-requestid"];
  return headers;
}
std::string userAgent(const Json& headers) {
  const auto& agent = value::property(headers,"user-agent");
  return agent.is_string() ? agent.get<std::string>() : runtime::userAgent();
}
} // namespace
Json bedrockBody(const Json& anthropicBody) {
  Json input = anthropicBody;
  Json betas = input.contains("betas") ? input["betas"] : Json(nullptr);
  input.erase("betas");
  auto body = transport::prepareMessageRequest(input,Json::object()).body;
  body.erase("model");
  body.erase("stream");
  if (!body.contains("anthropic_version")) body["anthropic_version"] = bedrockAnthropicVersion;
  if (betas.is_string()) betas = Json::array({betas});
  if (betas.is_array() && !betas.empty() && !body.contains("anthropic_beta")) body["anthropic_beta"] = betas;
  return body;
}
std::string bedrockModelPath(const std::string& modelId, bool stream) {
  return "/model/"+cloud::percentEncodeComponent(modelId)+(stream ? "/invoke-with-response-stream" : "/invoke");
}
std::string bedrockRuntimeUrl(const BedrockConfig& config, const std::string& region) {
  if (!config.baseUrl.empty()) return config.baseUrl;
  if (auto url = env("AWS_ENDPOINT_URL_BEDROCK_RUNTIME"); !url.empty()) return url;
  if (auto url = env("AWS_ENDPOINT_URL"); !url.empty()) return url;
  return "https://bedrock-runtime."+region+".amazonaws.com";
}
std::string bedrockControlUrl(const std::string& region) {
  if (auto url = env("AWS_ENDPOINT_URL_BEDROCK"); !url.empty()) return url;
  if (auto url = env("AWS_ENDPOINT_URL"); !url.empty()) return url;
  return "https://bedrock."+region+".amazonaws.com";
}
cloud::HttpRequest authorizeBedrockRequest(cloud::HttpRequest request, const BedrockAuth& auth, const std::string& region,
                                           std::chrono::system_clock::time_point now) {
  if (auth.kind == BedrockAuth::Kind::Bearer) {
    request.headers.emplace_back("authorization","Bearer "+auth.bearerToken);
    return request;
  }
  const auto parts = cloud::splitUrl(request.url);
  SigningRequest signing;
  signing.method = request.method;
  signing.target = parts.target;
  signing.body = request.body;
  signing.headers.emplace_back("host",parts.host);
  HeaderList unsignedHeaders;
  for (const auto& header : request.headers) {
    const auto& name = header.first;
    if (name == "content-type" || name == "accept" || name.starts_with("x-amz")) signing.headers.push_back(header);
    else unsignedHeaders.push_back(header);
  }
  SigningOptions options;
  options.region = region;
  options.service = "bedrock";
  options.time = now;
  auto result = sign(signing,auth.credentials,options);
  request.headers = std::move(unsignedHeaders);
  for (auto& header : result.headers) if (header.first != "host") request.headers.push_back(std::move(header));
  return request;
}
cloud::HttpRequest prepareBedrockRequest(const BedrockConfig& config, const BedrockAuth& auth, const std::string& region,
                                         const Json& anthropicBody, bool stream, std::chrono::system_clock::time_point now) {
  const auto& model = value::property(anthropicBody,"model");
  if (!model.is_string() || model.get<std::string>().empty()) throw std::invalid_argument("Bedrock requests need a model id");
  auto headers = transport::prepareMessageRequest(Json::object(),config.headers).headers;
  cloud::HttpRequest request;
  request.method = "POST";
  request.stream = stream;
  request.url = cloud::joinUrl(bedrockRuntimeUrl(config,region),bedrockModelPath(model.get<std::string>(),stream));
  request.body = value::stringify(bedrockBody(anthropicBody));
  request.headers = {{"content-type","application/json"},
                     {"accept",stream ? "application/vnd.amazon.eventstream" : "application/json"},
                     {"user-agent",userAgent(headers)}};
  for (auto it = headers.begin(); it != headers.end(); ++it) {
    if (it.key() == "content-type" || it.key() == "accept" || it.key() == "user-agent" || it.key() == "authorization" ||
        it.key() == "host" || it.value().is_null()) continue;
    request.headers.emplace_back(it.key(),it.value().get<std::string>());
  }
  return authorizeBedrockRequest(std::move(request),auth,region,now);
}
void throwBedrockHttpError(long status, const std::string& body, Json headers) {
  auto parsed = value::parse(body,false);
  if (parsed.is_discarded()) parsed = nullptr;
  std::string type;
  if (const auto& header = value::property(headers,"x-amzn-errortype"); header.is_string()) type = header.get<std::string>();
  if (type.empty()) if (const auto& field = value::property(parsed,"__type"); field.is_string()) type = field.get<std::string>();
  std::string message;
  for (const auto* key : {"message","Message"})
    if (const auto& field = value::property(parsed,key); field.is_string()) { message = field.get<std::string>(); break; }
  if (message.empty() && !parsed.is_object()) message = body.substr(0,2000);
  cloud::throwMapped(cloud::mapAwsException(type,status,message),withRequestId(std::move(headers)));
}
void BedrockStreamTranslator::handle(const EventStreamMessage& message, const transport::OnEvent& onEvent) {
  const auto kind = message.header(":message-type");
  if (kind == "event") {
    if (message.header(":event-type") != "chunk") return;
    const auto payload = value::parse(message.payload,false);
    const auto& bytes = value::property(payload,"bytes");
    if (!bytes.is_string()) throw EventStreamError("Bedrock chunk event has no bytes field");
    const auto decoded = cloud::base64Decode(bytes.get<std::string>());
    if (!decoded) throw EventStreamError("Bedrock chunk bytes are not valid base64");
    auto event = value::parse(*decoded,false);
    if (event.is_discarded() || !event.is_object()) throw EventStreamError("Bedrock chunk is not a JSON event");
    if (event.value("type","") == "error") throw transport::APIError(std::nullopt,event,headers,value::stringify(event));
    if (onEvent) onEvent(event);
    return;
  }
  if (kind == "exception") {
    const auto payload = value::parse(message.payload,false);
    std::string text;
    for (const auto* key : {"message","Message"})
      if (const auto& field = value::property(payload,key); field.is_string()) { text = field.get<std::string>(); break; }
    cloud::throwMapped(cloud::mapAwsException(message.header(":exception-type"),500,text),headers);
  }
  if (kind == "error") cloud::throwMapped(cloud::mapAwsException(message.header(":error-code"),500,message.header(":error-message")),headers);
}
void BedrockStreamTranslator::feed(std::string_view bytes, const transport::OnEvent& onEvent) {
  for (const auto& message : decoder.decode(bytes)) handle(message,onEvent);
}
void BedrockStreamTranslator::finish() const {
  if (decoder.pending())
    throw transport::APIConnectionError("Bedrock event stream ended inside a message ("+std::to_string(decoder.pendingBytes())+" bytes left)");
}
BedrockClient::BedrockClient(BedrockConfig settings, CredentialOptions credentialOptions)
  : config(std::move(settings)),
    credentials([&] {
      if (credentialOptions.bearerToken.empty()) credentialOptions.bearerToken = config.bearerToken;
      credentialOptions.network = config.network;
      return std::make_shared<CredentialChain>(std::move(credentialOptions));
    }()),
    now([] { return std::chrono::system_clock::now(); }),
    http(config.network) {}
BedrockClient::BedrockClient(BedrockConfig settings, std::shared_ptr<CredentialChain> chain)
  : config(std::move(settings)), credentials(std::move(chain)), now([] { return std::chrono::system_clock::now(); }),
    http(config.network) {
  if (!credentials) throw std::invalid_argument("BedrockClient needs a credential chain");
}
std::string BedrockClient::region() {
  if (!config.region.empty()) return config.region;
  if (auto region = credentials->region(); !region.empty()) return region;
  return "us-east-1";
}
transport::Response BedrockClient::request(const Json& anthropicBody, bool stream, const transport::OnEvent& onEvent, std::stop_token stop,
                                           const transport::OnResponse& onResponse, transport::Timeout timeout) {
  std::lock_guard lock(mutex);
  if (stop.stop_requested()) throw transport::APIUserAbortError();
  if (stream && !onEvent) throw std::invalid_argument("Streaming requests require an event consumer");
  const auto signingRegion = region();
  for (int attempt = 0;; ++attempt) {
    BedrockAuth auth;
    try { auth = credentials->resolve(); }
    catch (const std::exception& error) { cloud::throwMapped({401,"authentication_error",error.what()},Json::object()); }
    auto prepared = prepareBedrockRequest(config,auth,signingRegion,anthropicBody,stream,now());
    prepared.timeout = timeout;
    transport::Response response;
    std::optional<BedrockStreamTranslator> translator;
    const auto onHeaders = [&](long status, const Json& headers) {
      response.status = status;
      response.headers = withRequestId(headers);
      if (status >= 200 && status < 300) {
        translator.emplace(response.headers);
        if (onResponse) onResponse(response);
      }
    };
    const auto onBytes = [&](std::string_view bytes) {
      if (!translator) translator.emplace(response.headers);
      translator->feed(bytes,onEvent);
    };
    auto result = http.perform(prepared,stop,onHeaders,stream ? cloud::OnBytes(onBytes) : cloud::OnBytes());
    response.status = result.status;
    response.headers = withRequestId(result.headers);
    if (result.status < 200 || result.status >= 300) {
      try { throwBedrockHttpError(result.status,result.body,response.headers); }
      catch (const transport::APIError& error) {
        if (attempt == 0 && auth.kind == BedrockAuth::Kind::SigV4 && authFailure(error.body)) {
          credentials->invalidate();
          continue;
        }
        throw;
      }
    }
    if (stream) {
      if (translator) translator->finish();
    } else if (!result.body.empty()) {
      response.body = value::parse(result.body);
    }
    return response;
  }
}
cloud::HttpResult BedrockClient::signedGet(const std::string& url, const BedrockAuth& auth, const std::string& signingRegion) {
  cloud::HttpRequest request;
  request.method = "GET";
  request.url = url;
  request.timeout = std::chrono::seconds(20);
  request.headers = {{"accept","application/json"},{"user-agent",userAgent(config.headers)}};
  return http.perform(authorizeBedrockRequest(std::move(request),auth,signingRegion,now()));
}
Json BedrockClient::listModels() {
  std::lock_guard lock(mutex);
  const auto signingRegion = region();
  BedrockAuth auth;
  try { auth = credentials->resolve(); }
  catch (const std::exception& error) { cloud::throwMapped({401,"authentication_error",error.what()},Json::object()); }
  const auto control = bedrockControlUrl(signingRegion);
  Json data = Json::array();
  std::set<std::string> seen;
  const auto add = [&](const std::string& id, const std::string& name) {
    if (id.empty() || !seen.insert(id).second) return;
    Json row{{"id",id}};
    if (!name.empty()) row["display_name"] = name;
    data.push_back(std::move(row));
  };
  // System inference profiles (us.anthropic..., global.anthropic...) are what newer models need on demand.
  std::string nextToken;
  for (int page = 0; page < 20; ++page) {
    auto url = cloud::joinUrl(control,"/inference-profiles?maxResults=1000&typeEquals=SYSTEM_DEFINED");
    if (!nextToken.empty()) url += "&nextToken="+cloud::percentEncodeComponent(nextToken);
    const auto result = signedGet(url,auth,signingRegion);
    if (result.status != 200) break;
    const auto json = value::parse(result.body,false);
    for (const auto& profile : value::property(json,"inferenceProfileSummaries")) {
      const auto id = profile.value("inferenceProfileId","");
      if (id.find("anthropic.") != std::string::npos && profile.value("status","ACTIVE") == "ACTIVE")
        add(id,profile.value("inferenceProfileName",""));
    }
    const auto& token = value::property(json,"nextToken");
    if (!token.is_string() || token.get<std::string>().empty()) break;
    nextToken = token.get<std::string>();
  }
  const auto result = signedGet(cloud::joinUrl(control,"/foundation-models?byProvider=anthropic"),auth,signingRegion);
  if (result.status != 200) throwBedrockHttpError(result.status,result.body,result.headers);
  const auto json = value::parse(result.body,false);
  for (const auto& model : value::property(json,"modelSummaries")) {
    const auto& types = value::property(model,"inferenceTypesSupported");
    bool onDemand = !types.is_array();
    if (types.is_array()) for (const auto& type : types) onDemand = onDemand || type == "ON_DEMAND";
    if (onDemand) add(model.value("modelId",""),model.value("modelName",""));
  }
  return Json{{"data",std::move(data)}};
}
} // namespace batbox::providers::aws
