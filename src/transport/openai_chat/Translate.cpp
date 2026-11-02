#include "transport/openai_chat/Translate.hpp"
#include "transport/RawHttp.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <regex>

namespace batbox::transport::openai_chat {
namespace {
constexpr std::string_view kAlphabet = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
const Json& field(const Json& object, const char* name) { return value::property(object,name); }
std::string text(const Json& object, const char* name) {
  const auto& value = field(object,name);
  return value.is_string() ? value.get<std::string>() : std::string{};
}
std::int64_t integer(const Json& value) {
  if (value.is_number_integer() || value.is_number_unsigned()) return value.get<std::int64_t>();
  if (value.is_number_float()) return static_cast<std::int64_t>(std::llround(value.get<double>()));
  return 0;
}
bool idCharacters(const std::string& id, bool alnumOnly) {
  return std::all_of(id.begin(),id.end(),[alnumOnly](unsigned char c) {
    return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||(!alnumOnly&&(c=='_'||c=='-'));
  });
}
std::uint64_t fnv1a(const std::string& text, std::uint64_t salt) {
  std::uint64_t hash = 1469598103934665603ull ^ salt;
  for (unsigned char c : text) { hash ^= c; hash *= 1099511628211ull; }
  return hash;
}
std::string base62(std::uint64_t value, std::size_t length) {
  std::string out(length,'0');
  for (std::size_t i = 0; i < length; ++i) { out[length-1-i] = kAlphabet[value % 62]; value /= 62; }
  return out;
}
std::string joinTexts(const std::vector<std::string>& parts, const char* separator) {
  std::string joined;
  for (const auto& part : parts) {
    if (part.empty()) continue;
    if (!joined.empty()) joined += separator;
    joined += part;
  }
  return joined;
}
std::optional<Json> imagePart(const Json& block) {
  const auto& source = field(block,"source");
  const auto type = text(source,"type");
  if (type == "base64") {
    const auto media = text(source,"media_type");
    const auto data = text(source,"data");
    if (data.empty()) return std::nullopt;
    return Json{{"type","image_url"},{"image_url",{{"url","data:"+(media.empty()?"image/png":media)+";base64,"+data}}}};
  }
  if (type == "url" && !text(source,"url").empty())
    return Json{{"type","image_url"},{"image_url",{{"url",text(source,"url")}}}};
  return std::nullopt;
}
std::string documentText(const Json& block) {
  const auto& source = field(block,"source");
  if (text(source,"type") == "text") return text(source,"data");
  const auto title = text(block,"title");
  return "[Document" + (title.empty() ? std::string{} : " \"" + title + "\"") +
    " omitted: this endpoint takes text and images only]";
}
// Plain string when every part is text; a parts array once an image is present.
Json userContent(const Json& parts) {
  std::vector<std::string> texts;
  for (const auto& part : parts) {
    if (text(part,"type") != "text") return parts;
    texts.push_back(text(part,"text"));
  }
  return joinTexts(texts,"\n\n");
}
std::string toolResultText(const Json& block, Json& hoisted) {
  const auto& content = field(block,"content");
  std::string result;
  if (content.is_string()) result = content.get<std::string>();
  else if (content.is_array()) {
    std::vector<std::string> texts;
    for (const auto& item : content) {
      const auto type = text(item,"type");
      if (type == "image") { if (auto image = imagePart(item)) hoisted.push_back(*image); }
      else if (type == "document") texts.push_back(documentText(item));
      else if (field(item,"text").is_string()) texts.push_back(text(item,"text"));
    }
    result = joinTexts(texts,"\n");
  }
  if (field(block,"is_error") == true && !result.starts_with("Error")) result = "Error: " + result;
  return result;
}
void translateUser(const Json& blocks, Json& messages, ToolIdMap& ids) {
  Json parts = Json::array(), hoisted = Json::array();
  for (const auto& block : blocks) {
    const auto type = text(block,"type");
    if (type == "text") { if (!text(block,"text").empty()) parts.push_back(Json{{"type","text"},{"text",text(block,"text")}}); }
    else if (type == "image") { if (auto image = imagePart(block)) parts.push_back(*image); }
    else if (type == "document") parts.push_back(Json{{"type","text"},{"text",documentText(block)}});
    else if (type == "tool_result")
      messages.push_back(Json{{"role","tool"},{"tool_call_id",ids.outgoing(text(block,"tool_use_id"))},
        {"content",toolResultText(block,hoisted)}});
    else if (field(block,"text").is_string() && !text(block,"text").empty())
      parts.push_back(Json{{"type","text"},{"text",text(block,"text")}});
  }
  if (!hoisted.empty()) {
    Json withImages = Json::array({Json{{"type","text"},{"text","Images returned by the tool calls above:"}}});
    for (auto& image : hoisted) withImages.push_back(std::move(image));
    for (auto& part : parts) withImages.push_back(std::move(part));
    parts = std::move(withImages);
  }
  if (!parts.empty()) messages.push_back(Json{{"role","user"},{"content",userContent(parts)}});
}
void translateAssistant(const Json& blocks, Json& messages, ToolIdMap& ids, const ChatQuirks& quirks) {
  std::string content, reasoning;
  Json calls = Json::array();
  for (const auto& block : blocks) {
    const auto type = text(block,"type");
    if (type == "text") content += text(block,"text");
    else if (type == "thinking") reasoning += text(block,"thinking");
    else if (type == "tool_use") {
      const auto& input = field(block,"input");
      const auto arguments = input.is_string() ? input.get<std::string>() :
        (input.is_null() ? std::string("{}") : input.dump());
      calls.push_back(Json{{"id",ids.outgoing(text(block,"id"))},{"type","function"},
        {"function",{{"name",text(block,"name")},{"arguments",arguments}}}});
    }
  }
  Json message{{"role","assistant"}};
  message["content"] = content.empty() && !calls.empty() ? Json(nullptr) : Json(content);
  if (!calls.empty()) message["tool_calls"] = std::move(calls);
  if (!reasoning.empty()) {
    if (quirks.reasoning == "reasoning_content") message["reasoning_content"] = reasoning;
    else if (quirks.reasoning == "openrouter") message["reasoning"] = reasoning;
  }
  messages.push_back(std::move(message));
}
std::string effortName(const std::string& effort) {
  if (effort == "max" || effort == "xhigh") return "high";
  return effort;
}
std::string budgetEffort(const Json& budget) {
  const auto tokens = integer(budget);
  if (tokens >= 10000) return "high";
  if (tokens >= 5000) return "medium";
  return "low";
}
void translateReasoning(const Json& body, Json& out, const ChatQuirks& quirks) {
  const auto& thinking = field(body,"thinking");
  const auto type = text(thinking,"type");
  const auto effort = text(field(body,"output_config"),"effort");
  if (quirks.reasoning == "openai") {
    if (type == "enabled") out["reasoning_effort"] = effort.empty() ? budgetEffort(field(thinking,"budget_tokens")) : effortName(effort);
    else if ((type == "adaptive" || type.empty()) && !effort.empty()) out["reasoning_effort"] = effortName(effort);
    else if (type == "adaptive") out["reasoning_effort"] = "medium";
  } else if (quirks.reasoning == "openrouter") {
    if (type == "enabled" && effort.empty()) out["reasoning"] = {{"max_tokens",integer(field(thinking,"budget_tokens"))}};
    else if (type == "enabled" || type == "adaptive" || !effort.empty())
      out["reasoning"] = {{"effort",effort.empty() ? std::string("medium") : effortName(effort)}};
    else if (type == "disabled") out["reasoning"] = {{"enabled",false}};
  } else if (quirks.reasoning == "reasoning_content") {
    if (type == "enabled" || type == "adaptive") out["chat_template_kwargs"] = {{"enable_thinking",true}};
    else if (type == "disabled") out["chat_template_kwargs"] = {{"enable_thinking",false}};
  }
}
Json translateTools(const Json& tools) {
  Json functions = Json::array();
  if (!tools.is_array()) return functions;
  for (const auto& tool : tools) {
    const auto type = text(tool,"type");
    if (!type.empty() && type != "custom" && type != "function") continue;
    const auto name = text(tool,"name");
    if (name.empty()) continue;
    Json function{{"name",name}};
    if (!text(tool,"description").empty()) function["description"] = text(tool,"description");
    const auto& schema = field(tool,"input_schema");
    function["parameters"] = schema.is_object() ? schema : Json{{"type","object"},{"properties",Json::object()}};
    functions.push_back(Json{{"type","function"},{"function",std::move(function)}});
  }
  return functions;
}
std::optional<Json> translateToolChoice(const Json& choice) {
  const auto type = text(choice,"type");
  if (type == "auto") return Json("auto");
  if (type == "any") return Json("required");
  if (type == "none") return Json("none");
  if (type == "tool" && !text(choice,"name").empty())
    return Json{{"type","function"},{"function",{{"name",text(choice,"name")}}}};
  return std::nullopt;
}
std::string contextMessage(const std::string& message, const Json& body) {
  const auto& error = field(body,"error");
  const auto prompt = integer(field(error,"n_prompt_tokens"));
  const auto context = integer(field(error,"n_ctx"));
  if (prompt > 0 && context > 0)
    return "prompt is too long: " + std::to_string(prompt) + " tokens > " + std::to_string(context) +
      " maximum (" + message + ")";
  static const std::regex requested(R"(maximum context length is (\d+) tokens\. However, you requested (\d+) tokens \((\d+) in the messages, (\d+) in the completion\))");
  static const std::regex tooLarge(R"(is too large: (\d+)\. This model's maximum context length is (\d+) tokens and your request has (\d+) input tokens)");
  static const std::regex inputOnly(R"(maximum context length is (\d+) tokens\. However, your (?:request has|messages resulted in) (\d+) (?:input )?tokens)");
  std::smatch match;
  std::int64_t input = 0, output = 0, limit = 0;
  if (std::regex_search(message,match,requested)) {
    limit = std::stoll(match[1].str()); input = std::stoll(match[3].str()); output = std::stoll(match[4].str());
  } else if (std::regex_search(message,match,tooLarge)) {
    output = std::stoll(match[1].str()); limit = std::stoll(match[2].str()); input = std::stoll(match[3].str());
  } else if (std::regex_search(message,match,inputOnly)) {
    limit = std::stoll(match[1].str()); input = std::stoll(match[2].str());
  } else return message;
  if (input >= limit || output == 0)
    return "prompt is too long: " + std::to_string(input) + " tokens > " + std::to_string(limit) +
      " maximum (" + message + ")";
  return "input length and `max_tokens` exceed context limit: " + std::to_string(input) + " + " +
    std::to_string(output) + " > " + std::to_string(limit) + " (" + message + ")";
}
std::mt19937_64& generator() {
  thread_local std::mt19937_64 engine{std::random_device{}()};
  return engine;
}
} // namespace

ToolIdMap::ToolIdMap(std::string style) : style_(std::move(style)) {}
std::string ToolIdMap::outgoing(const std::string& anthropicId) {
  if (const auto found = ids_.find(anthropicId); found != ids_.end()) return found->second;
  const bool shortIds = style_ == "short";
  std::string candidate;
  if (shortIds ? anthropicId.size() == 9 && idCharacters(anthropicId,true)
               : !anthropicId.empty() && anthropicId.size() <= 40 && idCharacters(anthropicId,false))
    candidate = anthropicId;
  for (std::uint64_t salt = 0; candidate.empty() || used_.contains(candidate); ++salt) {
    const auto hash = fnv1a(anthropicId.empty() ? std::to_string(ids_.size()) : anthropicId,salt);
    candidate = shortIds ? base62(hash,9) : "call_" + base62(hash,11);
  }
  used_.insert(candidate);
  ids_.emplace(anthropicId,candidate);
  return candidate;
}

std::string randomId(const std::string& prefix) {
  std::uniform_int_distribution<std::size_t> pick(0,kAlphabet.size()-1);
  std::string id = prefix;
  for (int i = 0; i < 24; ++i) id += kAlphabet[pick(generator())];
  return id;
}
std::string incomingToolId(const std::string& serverId, std::set<std::string>& seen) {
  std::string id = serverId.size() >= 8 && serverId.size() <= 40 && idCharacters(serverId,false) ? serverId : std::string{};
  while (id.empty() || seen.contains(id)) id = randomId("toolu_");
  seen.insert(id);
  return id;
}

Json translateRequest(const Json& body, bool stream, const ChatQuirks& quirks) {
  Json out = Json::object();
  out["model"] = field(body,"model");
  Json messages = Json::array();
  const auto& system = field(body,"system");
  std::string systemText;
  if (system.is_string()) systemText = system.get<std::string>();
  else if (system.is_array()) {
    std::vector<std::string> texts;
    for (const auto& block : system) if (text(block,"type") == "text") texts.push_back(text(block,"text"));
    systemText = joinTexts(texts,"\n\n");
  }
  if (!systemText.empty()) messages.push_back(Json{{"role",quirks.systemRole},{"content",systemText}});
  ToolIdMap ids(quirks.toolIds);
  const auto& history = field(body,"messages");
  if (history.is_array()) for (const auto& message : history) {
    const auto role = text(message,"role");
    const auto& content = field(message,"content");
    if (content.is_string()) { messages.push_back(Json{{"role",role},{"content",content}}); continue; }
    if (!content.is_array()) continue;
    if (role == "assistant") translateAssistant(content,messages,ids,quirks);
    else translateUser(content,messages,ids);
  }
  out["messages"] = std::move(messages);
  auto tools = translateTools(field(body,"tools"));
  if (!tools.empty()) {
    out["tools"] = std::move(tools);
    const auto& choice = field(body,"tool_choice");
    if (auto translated = translateToolChoice(choice)) out["tool_choice"] = *translated;
    if (field(choice,"disable_parallel_tool_use") == true) out["parallel_tool_calls"] = false;
  }
  if (field(body,"max_tokens").is_number()) out[quirks.maxTokensField] = field(body,"max_tokens");
  for (const auto* name : {"temperature","top_p"})
    if (field(body,name).is_number()) out[name] = field(body,name);
  const auto& stops = field(body,"stop_sequences");
  if (stops.is_array() && !stops.empty()) out["stop"] = stops;
  translateReasoning(body,out,quirks);
  const auto& format = field(field(body,"output_config"),"format");
  if (text(format,"type") == "json_schema" && field(format,"schema").is_object())
    out["response_format"] = {{"type","json_schema"},{"json_schema",{{"name","output"},
      {"schema",field(format,"schema")},{"strict",true}}}};
  const auto user = text(field(body,"metadata"),"user_id");
  if (!user.empty()) out["user"] = user;
  out["stream"] = stream;
  if (stream && quirks.usageInStream) out["stream_options"] = {{"include_usage",true}};
  return out;
}

Json translateUsage(const Json& usage) {
  const auto prompt = integer(field(usage,"prompt_tokens"));
  auto cached = integer(field(field(usage,"prompt_tokens_details"),"cached_tokens"));
  if (cached == 0) cached = integer(field(usage,"prompt_cache_hit_tokens"));
  if (cached == 0) cached = integer(field(usage,"cached_tokens"));
  cached = std::clamp<std::int64_t>(cached,0,prompt);
  return Json{{"input_tokens",prompt-cached},{"output_tokens",integer(field(usage,"completion_tokens"))},
    {"cache_read_input_tokens",cached},{"cache_creation_input_tokens",0}};
}

std::string stopReason(const std::string& finishReason, bool sawToolCall) {
  if (finishReason == "length") return "max_tokens";
  if (finishReason == "content_filter") return "refusal";
  if (sawToolCall || finishReason == "tool_calls" || finishReason == "function_call") return "tool_use";
  return "end_turn";
}

Json translateResponse(const Json& chat, const std::string& model) {
  const auto& choices = field(chat,"choices");
  const Json& choice = choices.is_array() && !choices.empty() ? choices[0] : Json(nullptr);
  const auto& message = field(choice,"message");
  Json content = Json::array();
  auto reasoning = text(message,"reasoning_content");
  if (reasoning.empty()) reasoning = text(message,"reasoning");
  if (!reasoning.empty()) content.push_back(Json{{"type","thinking"},{"thinking",reasoning},{"signature",""}});
  std::string reply;
  const auto& body = field(message,"content");
  if (body.is_string()) reply = body.get<std::string>();
  else if (body.is_array()) for (const auto& part : body) reply += text(part,"text");
  const auto refusal = text(message,"refusal");
  if (!reply.empty()) content.push_back(Json{{"type","text"},{"text",reply}});
  else if (!refusal.empty()) content.push_back(Json{{"type","text"},{"text",refusal}});
  std::set<std::string> seen;
  bool sawToolCall = false;
  const auto& calls = field(message,"tool_calls");
  if (calls.is_array()) for (const auto& call : calls) {
    const auto& function = field(call,"function");
    const auto& arguments = field(function,"arguments");
    Json input = Json::object();
    if (arguments.is_object()) input = arguments;
    else if (arguments.is_string() && !value::trim(arguments.get<std::string>()).empty()) {
      auto parsed = value::parse(arguments.get<std::string>(),false);
      if (!parsed.is_discarded() && parsed.is_object()) input = std::move(parsed);
    }
    content.push_back(Json{{"type","tool_use"},{"id",incomingToolId(text(call,"id"),seen)},
      {"name",text(function,"name")},{"input",std::move(input)}});
    sawToolCall = true;
  }
  auto reason = stopReason(text(choice,"finish_reason"),sawToolCall);
  if (reply.empty() && !refusal.empty()) reason = "refusal";
  return Json{{"id",randomId("msg_")},{"type","message"},{"role","assistant"},{"model",model},
    {"content",std::move(content)},{"stop_reason",reason},{"stop_sequence",nullptr},
    {"usage",translateUsage(field(chat,"usage"))}};
}

std::string anthropicErrorType(std::optional<long> status, const Json& body) {
  if (!status || *status == 0) {
    const auto& code = field(field(body,"error"),"code");
    if (code.is_number()) status = static_cast<long>(integer(code));
  }
  if (status) {
    if (*status == 401 || *status == 403) return "authentication_error";
    if (*status == 429) return "rate_limit_error";
    if (*status == 503 || *status == 529) return "overloaded_error";
    if (*status >= 500) return "api_error";
    if (*status >= 400) return "invalid_request_error";
  }
  const auto type = text(field(body,"error"),"type") + " " + text(field(body,"error"),"code");
  if (type.find("overloaded") != std::string::npos) return "overloaded_error";
  if (type.find("rate_limit") != std::string::npos) return "rate_limit_error";
  return "api_error";
}

APIError translateError(std::optional<long> status, const Json& body, const std::string& raw,
                        const Json& headers, const std::string& baseUrl) {
  auto message = errorText(body,raw);
  if (message.empty()) message = status ? "HTTP " + std::to_string(*status) + " from the server" : "Server reported an error";
  if (!status || *status == 400 || *status == 413 || *status == 422) message = contextMessage(message,body);
  const Json anthropic{{"type","error"},{"error",{{"type",anthropicErrorType(status,body)},{"message",message}}}};
  return readableAuthError(APIError(status,anthropic,headers),baseUrl);
}
} // namespace batbox::transport::openai_chat
