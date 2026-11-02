#include "transport/openai_chat/OpenAIChatClient.hpp"
#include "transport/openai_chat/StreamDecoder.hpp"
#include "transport/openai_chat/Translate.hpp"
#include "transport/SSE.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::transport::openai_chat {
namespace {
// OpenAI-style servers name the request id x-request-id; the model layer
// reads request-id.
Json withRequestId(Json headers) {
  if (!headers.contains("request-id") && headers.contains("x-request-id")) headers["request-id"] = headers["x-request-id"];
  return headers;
}
} // namespace

OpenAIChatClient::OpenAIChatClient(ClientConfig config, ChatQuirks quirks)
  : quirks_(std::move(quirks)), session_(std::move(config)) {}

Response OpenAIChatClient::request(const Json& anthropicBody, bool stream, const OnEvent& onEvent,
    std::stop_token stop, const OnResponse& onResponse, Timeout timeout) {
  if (stream && !onEvent) throw std::invalid_argument("Streaming requests require an event consumer");
  const auto& config = session_.config();
  const auto& modelField = value::property(anthropicBody,"model");
  const std::string model = modelField.is_string() ? modelField.get<std::string>() : std::string{};
  RawRequest request;
  request.url = joinUrl(config.baseUrl,config.messagesPath);
  request.headers = requestHeaders(config,stream);
  request.body = value::stringify(translateRequest(anthropicBody,stream,quirks_));
  request.stream = stream;
  const auto reportHeaders = [&onResponse](const Response& response) {
    if (onResponse) onResponse(Response{response.status,withRequestId(response.headers),nullptr});
  };
  SSEDecoder frames;
  StreamDecoder decoder(model,request.body.size(),config.baseUrl);
  const auto deliver = [&](const std::vector<ServerSentEvent>& events) {
    for (const auto& event : events)
      for (const auto& translated : decoder.consume(event)) onEvent(translated);
  };
  std::function<void(std::string_view)> onData;
  if (stream) onData = [&](std::string_view bytes) { deliver(frames.decode(bytes)); };
  auto reply = session_.perform(request,onData,stop,reportHeaders,timeout);
  auto headers = withRequestId(std::move(reply.headers));
  if (reply.status < 200 || reply.status >= 300) {
    auto parsed = value::parse(reply.body,false);
    if (parsed.is_discarded()) parsed = nullptr;
    throw translateError(reply.status,parsed,reply.body,headers,config.baseUrl);
  }
  if (stream) {
    deliver(frames.flush());
    for (const auto& translated : decoder.finish()) onEvent(translated);
    return Response{reply.status,std::move(headers),nullptr};
  }
  auto parsed = value::parse(reply.body,false);
  if (parsed.is_discarded() || !parsed.is_object())
    throw APIError(reply.status,nullptr,headers,"The server's reply is not a JSON object");
  if (!value::property(parsed,"error").is_null())
    throw translateError(std::nullopt,parsed,reply.body,headers,config.baseUrl);
  return Response{reply.status,std::move(headers),translateResponse(parsed,model)};
}

Json OpenAIChatClient::listModels() {
  return fetchModelList(session_,modelsUrl(session_.config().baseUrl,session_.config().modelsPath));
}

std::string OpenAIChatClient::kind() { return std::string(kProviderOpenAIChat); }
} // namespace batbox::transport::openai_chat
