#include "streaming/Reducer.hpp"
#include "services/api/batbox.hpp"
#include "runtime/MessageSnapshot.hpp"
#include <chrono>
#include <stdexcept>
#include <utility>

namespace batbox::streaming {
namespace {
const Json& field(const Json& object, const char* name) { return value::property(object, name); }
std::string stringField(const Json& object, const char* name) {
  const auto& value = field(object, name);
  return value.is_string() ? value.get<std::string>() : std::string{};
}
Json parsedInput(const std::string& text) {
  if (text.empty()) return Json::object();
  try {
    std::string_view view{text};
    if (view.starts_with("\xef\xbb\xbf")) view.remove_prefix(3);
    Json parsed = value::parse(view);
    return parsed.is_null() ? Json::object() : parsed;
  } catch (const Json::parse_error&) {
    return Json::object();
  }
}
void appendString(Json& block, const char* name, const Json& fragment) {
  // SDK event fields are typed strings; deltas append to their current values.
  block[name].get_ref<Json::string_t&>().append(
    fragment.get_ref<const Json::string_t&>());
}
std::int64_t currentMillis() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
}
} // namespace

Json normalizeContentFromAPI(const Json& contentBlocks, const NormalizeGadgets& gadgets) {
  if (contentBlocks.is_null()) return Json::array();
  Json normalized = Json::array();
  for (const Json& contentBlock : contentBlocks) {
    const std::string type = stringField(contentBlock, "type");
    if (type == "tool_use") {
      const Json& input = field(contentBlock, "input");
      if (!input.is_string() && !input.is_object() && !input.is_array())
        throw StreamError(StreamErrorKind::InvalidGadgetInput, "Tool use input must be a string or object");
      Json normalizedInput = input.is_string() ? parsedInput(input.get<std::string>()) : input;
      if ((normalizedInput.is_object() || normalizedInput.is_array()) && gadgets.findGadgetByName) {
        auto gadget = gadgets.findGadgetByName(stringField(contentBlock, "name"));
        if (gadget && gadgets.normalizeGadgetInput) {
          try { normalizedInput = gadgets.normalizeGadgetInput(*gadget, normalizedInput, gadgets.batId); }
          catch (...) { /* Keep the input unchanged if parsing fails. */ }
        }
      }
      Json result = contentBlock;
      result["input"] = std::move(normalizedInput);
      normalized.push_back(std::move(result));
      continue;
    }
    if (type == "server_tool_use") {
      const Json& input = field(contentBlock, "input");
      if (input.is_string()) {
        Json result = contentBlock;
        result["input"] = parsedInput(input.get<std::string>());
        normalized.push_back(std::move(result));
        continue;
      }
    }
    // Text, beta-specific blocks, thinking and other block types pass through.
    // Whitespace-only text does not emit analytics.
    normalized.push_back(contentBlock);
  }
  return normalized;
}

Reducer::Reducer(ReducerConfig config) : config_(std::move(config)), usage_(EMPTY_USAGE) {
  if (!config_.randomUUID || !config_.timestamp || !config_.calculateUSDCost ||
      !config_.addToTotalFlightCost || !config_.getErrorMessageIfRefusal ||
      !config_.createAssistantAPIErrorMessage)
    throw std::invalid_argument("Streaming reducer requires message, clock and cost dependencies");
}

Emissions Reducer::consume(const Json& part){
  Emissions emitted;
  auto next=beginEvent(part);
  while(next){emitted.push_back(*next);next=nextEmission();}
  return emitted;
}

std::optional<MessageRef> Reducer::beginEvent(const Json& part){
  if(pendingEvent_)throw std::logic_error("Previous stream event still has emissions");
  if(stringField(part,"type")!="message_delta"){
    PendingEvent pending;
    pending.part=part;
    pending.phase=PendingEvent::Phase::Buffered;
    pending.buffered=consumeUnphased(part);
    pendingEvent_=std::move(pending);
    return nextEmission();
  }
  usage_=updateUsage(usage_,field(part,"usage"),config_.stashedMicrocompact);
  stopReason_=field(field(part,"delta"),"stop_reason");
  if(!newMessages_.empty()){
    runtime::MessageLock messageLock(newMessages_.back());
    Json& lastMessage=messageLock.value()["message"];
    lastMessage["usage"]=usage_;
    lastMessage["stop_reason"]=stopReason_;
  }
  const double partCost=config_.calculateUSDCost(config_.resolvedModel,usage_);
  costUSD_+=config_.addToTotalFlightCost(partCost,usage_,config_.model);
  auto refusal=config_.getErrorMessageIfRefusal(stopReason_,config_.model);
  PendingEvent pending;
  pending.part=part;
  pending.phase=PendingEvent::Phase::AfterRefusal;
  pendingEvent_=std::move(pending);
  if(refusal)return std::make_shared<Json>(std::move(*refusal));
  return nextEmission();
}

std::optional<MessageRef> Reducer::nextEmission(){
  if(!pendingEvent_)return std::nullopt;
  while(true){
    switch(pendingEvent_->phase){
      case PendingEvent::Phase::Buffered:
        if(pendingEvent_->index<pendingEvent_->buffered.size())
          return pendingEvent_->buffered[pendingEvent_->index++];
        pendingEvent_.reset();return std::nullopt;
      case PendingEvent::Phase::AfterRefusal:
        pendingEvent_->phase=PendingEvent::Phase::AfterMaxTokens;
        if(stopReason_=="max_tokens")
          return std::make_shared<Json>(config_.createAssistantAPIErrorMessage(
            "API Error: Batbox's response exceeded the "+
            std::to_string(config_.maxOutputTokens)+
            " output token maximum. To configure this behavior, set the "
            "BATBOX_MAX_OUTPUT_TOKENS environment variable.",
            "max_output_tokens","max_output_tokens"));
        continue;
      case PendingEvent::Phase::AfterMaxTokens:
        pendingEvent_->phase=PendingEvent::Phase::AfterContextError;
        if(stopReason_=="model_context_window_exceeded")
          return std::make_shared<Json>(config_.createAssistantAPIErrorMessage(
            "API Error: The model has reached its context window limit.",
            "max_output_tokens","max_output_tokens"));
        continue;
      case PendingEvent::Phase::AfterContextError: {
        pendingEvent_->phase=PendingEvent::Phase::RawDone;
        return std::make_shared<Json>(Json{{"type","stream_event"},
          {"event",pendingEvent_->part}});
      }
      case PendingEvent::Phase::RawDone:
        pendingEvent_.reset();return std::nullopt;
    }
  }
}

Emissions Reducer::consumeUnphased(const Json& part) {
  Emissions emitted;
  const std::string type = stringField(part, "type");
  if (type == "message_start") {
    partialMessage_ = field(part, "message");
    ttftMs_ = (config_.nowMillis ? config_.nowMillis() : currentMillis()) - config_.startMillis;
    usage_ = updateUsage(usage_, field(partialMessage_, "usage"), config_.stashedMicrocompact);
  } else if (type == "content_block_start") {
    const auto& original = field(part, "content_block");
    Json block = original;
    const std::string blockType = stringField(original, "type");
    if (blockType == "tool_use" || blockType == "server_tool_use") {
      block["input"] = "";
      if (blockType == "server_tool_use" && stringField(original, "name") == "advisor")
        isAdvisorInProgress_ = true;
    } else if (blockType == "text") {
      block["text"] = "";
    } else if (blockType == "thinking") {
      block["thinking"] = "";
      block["signature"] = "";
    } else if (blockType == "advisor_tool_result") {
      isAdvisorInProgress_ = false;
    }
    const auto index = field(part, "index").get<std::size_t>();
    if (contentBlocks_.size() <= index) contentBlocks_.resize(index + 1);
    contentBlocks_[index] = std::move(block);
  } else if (type == "content_block_delta") {
    const auto index = field(part, "index").get<std::size_t>();
    if (index >= contentBlocks_.size() || !contentBlocks_[index])
      throw StreamError(StreamErrorKind::MissingBlock, "Content block not found");
    Json& block = *contentBlocks_[index];
    const Json& delta = field(part, "delta");
    const std::string deltaType = stringField(delta, "type");
    const std::string blockType = stringField(block, "type");
    if (config_.connectorTextEnabled && deltaType == "connector_text_delta") {
      if (blockType != "connector_text") throw StreamError(StreamErrorKind::TypeMismatch, "Content block is not a connector_text block");
      appendString(block, "connector_text", field(delta, "connector_text"));
    } else if (deltaType == "input_json_delta") {
      if (blockType != "tool_use" && blockType != "server_tool_use")
        throw StreamError(StreamErrorKind::TypeMismatch, "Content block is not a input_json block");
      if (!field(block, "input").is_string()) throw StreamError(StreamErrorKind::TypeMismatch, "Content block input is not a string");
      appendString(block, "input", field(delta, "partial_json"));
    } else if (deltaType == "text_delta") {
      if (blockType != "text") throw StreamError(StreamErrorKind::TypeMismatch, "Content block is not a text block");
      appendString(block, "text", field(delta, "text"));
    } else if (deltaType == "signature_delta") {
      if (config_.connectorTextEnabled && blockType == "connector_text") {
        block["signature"] = field(delta, "signature");
      } else {
        if (blockType != "thinking") throw StreamError(StreamErrorKind::TypeMismatch, "Content block is not a thinking block");
        block["signature"] = field(delta, "signature");
      }
    } else if (deltaType == "thinking_delta") {
      if (blockType != "thinking") throw StreamError(StreamErrorKind::TypeMismatch, "Content block is not a thinking block");
      appendString(block, "thinking", field(delta, "thinking"));
    }
    // Ignore citations_delta and unknown delta types.
  } else if (type == "content_block_stop") {
    const auto index = field(part, "index").get<std::size_t>();
    if (index >= contentBlocks_.size() || !contentBlocks_[index])
      throw StreamError(StreamErrorKind::MissingBlock, "Content block not found");
    if (!value::truthy(partialMessage_)) throw StreamError(StreamErrorKind::MissingMessage, "Message not found");
    Json message = partialMessage_;
    Json singleBlock = Json::array({*contentBlocks_[index]});
    message["content"] = config_.normalizeContent ? config_.normalizeContent(singleBlock)
                                                    : normalizeContentFromAPI(singleBlock);
    Json result = Json::object();
    result["message"] = std::move(message);
    if (!config_.requestId.empty()) result["requestId"] = config_.requestId;
    result["type"] = "assistant";
    result["uuid"] = config_.randomUUID();
    result["timestamp"] = config_.timestamp();
    if (!config_.advisorModel.empty()) result["advisorModel"] = config_.advisorModel;
    auto ref = std::make_shared<Json>(std::move(result));
    newMessages_.push_back(ref);
    emitted.push_back(ref);
  } else if (type == "message_delta") {
    throw std::logic_error("message_delta requires phased reducer path");
  }
  // Every successfully processed part produces a raw stream event, including
  // unknown event types and message_stop.
  Json streamEvent = {{"type", "stream_event"}, {"event", part}};
  if (type == "message_start") streamEvent["ttftMs"] = ttftMs_;
  emitted.push_back(std::make_shared<Json>(std::move(streamEvent)));
  return emitted;
}

void Reducer::finish() const {
  if (!value::truthy(partialMessage_) || (newMessages_.empty() && !value::truthy(stopReason_)))
    throw StreamError(StreamErrorKind::IncompleteStream, "Stream ended without receiving any events");
}
} // namespace batbox::streaming
