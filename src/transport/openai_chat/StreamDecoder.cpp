#include "transport/openai_chat/StreamDecoder.hpp"
#include "transport/openai_chat/Translate.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::transport::openai_chat {
namespace {
const Json& field(const Json& object, const char* name) { return value::property(object,name); }
std::string text(const Json& object, const char* name) {
  const auto& value = field(object,name);
  return value.is_string() ? value.get<std::string>() : std::string{};
}
Json delta(std::size_t index, Json body) {
  return Json{{"type","content_block_delta"},{"index",index},{"delta",std::move(body)}};
}
std::string preview(const std::string& data) {
  return data.size() > 200 ? data.substr(0,200) + "..." : data;
}
} // namespace

StreamDecoder::StreamDecoder(std::string model, std::size_t requestBytes, std::string baseUrl)
  : model_(std::move(model)), requestBytes_(requestBytes), baseUrl_(std::move(baseUrl)) {}

std::vector<Json> StreamDecoder::consume(const ServerSentEvent& event) {
  if (event.event == "ping") return {};
  if (event.event == "error") {
    auto body = value::parse(event.data,false);
    if (body.is_discarded()) body = Json{{"error",{{"message",event.data}}}};
    throw translateError(std::nullopt,body,event.data,Json::object(),baseUrl_);
  }
  return consumeData(event.data);
}

std::vector<Json> StreamDecoder::consumeData(const std::string& data) {
  std::vector<Json> out;
  if (complete_) return out;
  const auto trimmed = value::trim(data);
  if (trimmed.empty()) return out;
  if (trimmed == "[DONE]") { end(out); return out; }
  auto chunk = value::parse(trimmed,false);
  if (chunk.is_discarded() || !chunk.is_object())
    throw std::runtime_error("Malformed stream chunk from the server: " + preview(trimmed));
  if (!field(chunk,"error").is_null())
    throw translateError(std::nullopt,chunk,trimmed,Json::object(),baseUrl_);
  start(out);
  if (field(chunk,"usage").is_object()) usage_ = field(chunk,"usage");
  const auto& choices = field(chunk,"choices");
  if (!choices.is_array() || choices.empty()) return out;
  const auto& choice = choices[0];
  if (field(choice,"usage").is_object()) usage_ = field(choice,"usage");
  const auto& change = field(choice,"delta");
  auto reasoning = text(change,"reasoning_content");
  if (reasoning.empty()) reasoning = text(change,"reasoning");
  if (!reasoning.empty()) {
    if (openKind_ != Block::Thinking) open(out,Block::Thinking,{{"type","thinking"},{"thinking",""},{"signature",""}});
    out.push_back(delta(openIndex_,{{"type","thinking_delta"},{"thinking",reasoning}}));
    generatedBytes_ += reasoning.size();
  }
  const auto content = text(change,"content");
  if (!content.empty()) {
    if (openKind_ != Block::Text) open(out,Block::Text,{{"type","text"},{"text",""}});
    out.push_back(delta(openIndex_,{{"type","text_delta"},{"text",content}}));
    generatedBytes_ += content.size();
  }
  const auto& calls = field(change,"tool_calls");
  if (calls.is_array()) for (const auto& call : calls) toolDelta(out,call);
  const auto reason = text(choice,"finish_reason");
  if (!reason.empty() && !finished_) {
    for (const auto& key : toolOrder_)
      if (!tools_.at(key).index)
        throw std::runtime_error("The server streamed a tool call without a function name");
    close(out);
    finishReason_ = reason;
    finished_ = true;
  }
  return out;
}

std::vector<Json> StreamDecoder::finish() {
  std::vector<Json> out;
  if (complete_) return out;
  if (!finished_) throw std::runtime_error("The server's stream ended before it finished the response");
  end(out);
  return out;
}

void StreamDecoder::start(std::vector<Json>& out) {
  if (started_) return;
  started_ = true;
  out.push_back(Json{{"type","message_start"},{"message",{{"id",randomId("msg_")},{"type","message"},
    {"role","assistant"},{"model",model_},{"content",Json::array()},{"stop_reason",nullptr},
    {"stop_sequence",nullptr},{"usage",{{"input_tokens",0},{"output_tokens",0}}}}}});
}

void StreamDecoder::open(std::vector<Json>& out, Block kind, Json contentBlock) {
  close(out);
  openKind_ = kind;
  openIndex_ = nextIndex_++;
  out.push_back(Json{{"type","content_block_start"},{"index",openIndex_},{"content_block",std::move(contentBlock)}});
}

void StreamDecoder::close(std::vector<Json>& out) {
  if (openKind_ == Block::None) return;
  if (openKind_ == Block::Thinking) out.push_back(delta(openIndex_,{{"type","signature_delta"},{"signature",""}}));
  if (openKind_ == Block::Tool && openToolKey_) tools_.at(*openToolKey_).closed = true;
  out.push_back(Json{{"type","content_block_stop"},{"index",openIndex_}});
  openKind_ = Block::None;
  openToolKey_.reset();
}

void StreamDecoder::toolDelta(std::vector<Json>& out, const Json& call) {
  std::string key;
  const auto& index = field(call,"index");
  const auto id = text(call,"id");
  if (index.is_number_integer() || index.is_number_unsigned()) key = "#" + std::to_string(index.get<long long>());
  else if (!id.empty()) {
    for (const auto& [existing,tool] : tools_) if (tool.serverId == id) key = existing;
    if (key.empty()) key = "id:" + id;
  } else if (!toolOrder_.empty()) key = toolOrder_.back();
  else key = "#0";
  auto [found,created] = tools_.try_emplace(key);
  auto& tool = found->second;
  if (created) toolOrder_.push_back(key);
  if (!id.empty() && tool.serverId.empty()) tool.serverId = id;
  const auto& function = field(call,"function");
  const auto name = text(function,"name");
  if (!name.empty() && tool.name.empty()) tool.name = name;
  const auto& rawArguments = field(function,"arguments");
  const std::string arguments = rawArguments.is_string() ? rawArguments.get<std::string>() :
    (rawArguments.is_object() ? rawArguments.dump() : std::string{});
  generatedBytes_ += arguments.size();
  if (!tool.index) {
    tool.pending += arguments;
    if (!tool.name.empty()) startTool(out,tool);
    if (tool.index) openToolKey_ = key;
    return;
  }
  if (arguments.empty()) return;
  if (tool.closed || openToolKey_ != key)
    throw std::runtime_error("The server interleaved arguments for parallel tool calls");
  out.push_back(delta(*tool.index,{{"type","input_json_delta"},{"partial_json",arguments}}));
}

void StreamDecoder::startTool(std::vector<Json>& out, Tool& tool) {
  open(out,Block::Tool,{{"type","tool_use"},{"id",incomingToolId(tool.serverId,toolIds_)},
    {"name",tool.name},{"input",Json::object()}});
  tool.index = openIndex_;
  sawToolCall_ = true;
  if (!tool.pending.empty()) {
    out.push_back(delta(openIndex_,{{"type","input_json_delta"},{"partial_json",tool.pending}}));
    tool.pending.clear();
  }
}

void StreamDecoder::end(std::vector<Json>& out) {
  if (complete_) return;
  for (const auto& key : toolOrder_)
    if (!tools_.at(key).index)
      throw std::runtime_error("The server streamed a tool call without a function name");
  start(out);
  close(out);
  Json usage;
  if (usage_.is_object()) usage = translateUsage(usage_);
  else usage = Json{{"input_tokens",static_cast<std::int64_t>((requestBytes_+3)/4)},
    {"output_tokens",static_cast<std::int64_t>((generatedBytes_+3)/4)},
    {"cache_read_input_tokens",0},{"cache_creation_input_tokens",0}};
  out.push_back(Json{{"type","message_delta"},{"delta",{{"stop_reason",stopReason(finishReason_,sawToolCall_)},
    {"stop_sequence",nullptr}}},{"usage",std::move(usage)}});
  out.push_back(Json{{"type","message_stop"}});
  finished_ = true;
  complete_ = true;
}
} // namespace batbox::transport::openai_chat
