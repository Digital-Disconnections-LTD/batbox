#include "interactive/StreamReducer.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::interactive {
namespace {
const Json* property(const Json& value, const char* key) {
  if (!value.is_object()) return nullptr;
  const auto it = value.find(key);
  return it == value.end() ? nullptr : &*it;
}
std::string typeOf(const Json& value) {
  if (const auto* type = property(value, "type"); type && type->is_string())
    return type->get<std::string>();
  return {};
}
const Json& field(const Json& value, const char* key) {
  if (const auto* found = property(value, key)) return *found;
  throw std::runtime_error(std::string("Stream event missing ") + key);
}
std::string textField(const Json& value, const char* key) {
  const auto& found = field(value, key);
  if (!found.is_string())
    throw std::runtime_error(std::string("Stream event requires string ") + key);
  return found.get<std::string>();
}
template <class F> void required(const F& callback, const char* name) {
  if (!callback) throw std::logic_error(std::string("Missing stream callback ") + name);
}
void mode(const StreamCallbacks& c, const char* value) {
  required(c.onSetStreamMode, "onSetStreamMode");
  c.onSetStreamMode(value);
}
} // namespace

void handleMessageFromStream(const Json& message,
    const StreamCallbacks& c, const StreamImports& imports) {
  const auto messageType = typeOf(message);
  if (messageType != "stream_event" && messageType != "stream_request_start") {
    if (messageType == "tombstone") {
      if (c.onTombstone) c.onTombstone(field(message, "message"));
      return;
    }
    if (messageType == "tool_use_summary") return;
    if (messageType == "assistant") {
      const auto& payload = field(message, "message");
      const auto& content = field(payload, "content");
      if (!content.is_array())
        throw std::runtime_error("Assistant stream content must be an array");
      for (const auto& block : content) {
        if (typeOf(block) != "thinking") continue;
        if (c.onStreamingThinking) {
          const auto thinking = textField(block, "thinking");
          const auto now = imports.nowMs;
          c.onStreamingThinking([thinking, now](const auto&) {
            required(now, "nowMs");
            return std::optional<StreamingThinking>{
                StreamingThinking{thinking, false, now()}};
          });
        }
        break;
      }
    }
    if (c.onStreamingText)
      c.onStreamingText([](const auto&) -> std::optional<std::string> {
        return std::nullopt;
      });
    required(c.onMessage, "onMessage");
    c.onMessage(message);
    return;
  }

  if (messageType == "stream_request_start") {
    mode(c, "requesting");
    return;
  }

  const auto& event = field(message, "event");
  const auto eventType = typeOf(event);
  if (eventType == "message_start") {
    if (const auto* ttft = property(message, "ttftMs");
        ttft && !ttft->is_null() && c.onApiMetrics)
      c.onApiMetrics(ttft->get<double>());
  }
  if (eventType == "message_stop") {
    mode(c, "tool-use");
    required(c.onStreamingGadgetUses, "onStreamingToolUses");
    c.onStreamingGadgetUses([](const auto&) {
      return std::vector<StreamingGadgetUse>{};
    });
    return;
  }

  if (eventType == "content_block_start") {
    if (c.onStreamingText)
      c.onStreamingText([](const auto&) -> std::optional<std::string> {
        return std::nullopt;
      });
    const auto& block = field(event, "content_block");
    required(imports.connectorTextEnabled, "connectorTextEnabled");
    if (imports.connectorTextEnabled()) {
      required(imports.isConnectorTextBlock, "isConnectorTextBlock");
      if (imports.isConnectorTextBlock(block)) {
        mode(c, "responding");
        return;
      }
    }
    const auto blockType = typeOf(block);
    if (blockType == "thinking" || blockType == "redacted_thinking") {
      mode(c, "thinking");
      return;
    }
    if (blockType == "text") {
      mode(c, "responding");
      return;
    }
    if (blockType == "tool_use") {
      mode(c, "tool-input");
      const auto index = field(event, "index");
      required(c.onStreamingGadgetUses, "onStreamingToolUses");
      c.onStreamingGadgetUses([index, block](const auto& current) {
        auto next = current;
        next.push_back(StreamingGadgetUse{index, block, ""});
        return next;
      });
      return;
    }
    if (blockType == "server_tool_use" ||
        blockType == "web_search_tool_result" ||
        blockType == "code_execution_tool_result" ||
        blockType == "mcp_tool_use" || blockType == "mcp_tool_result" ||
        blockType == "container_upload" ||
        blockType == "web_fetch_tool_result" ||
        blockType == "bash_code_execution_tool_result" ||
        blockType == "text_editor_code_execution_tool_result" ||
        blockType == "tool_search_tool_result" || blockType == "compaction")
      mode(c, "tool-input");
    return;
  }
  if (eventType == "content_block_delta") {
    const auto& delta = field(event, "delta");
    const auto deltaType = typeOf(delta);
    if (deltaType == "text_delta") {
      const auto text = textField(delta, "text");
      required(c.onUpdateLength, "onUpdateLength");
      c.onUpdateLength(text);
      if (c.appendStreamingText) c.appendStreamingText(text);
      else if (c.onStreamingText)
        c.onStreamingText([text](const auto& current) {
          return std::optional<std::string>{current.value_or("") + text};
        });
      return;
    }
    if (deltaType == "input_json_delta") {
      const auto fragment = textField(delta, "partial_json");
      const auto index = field(event, "index");
      required(c.onUpdateLength, "onUpdateLength");
      c.onUpdateLength(fragment);
      required(c.onStreamingGadgetUses, "onStreamingToolUses");
      if (c.appendGadgetInput) {
        c.appendGadgetInput(index, fragment);
        return;
      }
      c.onStreamingGadgetUses([fragment, index](const auto& current) {
        auto found = current.end();
        for (auto it = current.begin(); it != current.end(); ++it) {
          if (it->index == index) { found = it; break; }
        }
        if (found == current.end()) return current;
        std::vector<StreamingGadgetUse> next;
        next.reserve(current.size());
        for (auto it = current.begin(); it != current.end(); ++it)
          if (it != found) next.push_back(*it);
        auto updated = *found;
        updated.unparsedGadgetInput += fragment;
        next.push_back(std::move(updated));
        return next;
      });
      return;
    }
    if (deltaType == "thinking_delta") {
      required(c.onUpdateLength, "onUpdateLength");
      c.onUpdateLength(textField(delta, "thinking"));
      return;
    }
    return;
  }
  if (eventType == "content_block_stop") return;
  mode(c, "responding");
}
} // namespace batbox::interactive
