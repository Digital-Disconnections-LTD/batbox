#include "interactive/TranscriptRows.hpp"
#include "messages/TextContent.hpp"
#include "runtime/MessageMutation.hpp"
#include <algorithm>

namespace batbox::interactive {
namespace {
const Json* member(const Json& object, const char* key) {
  if (!object.is_object()) return nullptr;
  const auto found = object.find(key);
  return found == object.end() ? nullptr : &*found;
}
std::string string(const Json& object, const char* key) {
  const auto* value = member(object, key);
  return value && value->is_string() ? value->get<std::string>() : "";
}
std::string contentText(const Json& content) {
  if (content.is_string()) return content.get<std::string>();
  if (content.is_array()) return messages::extractTextContent(content, "\n");
  return "";
}
void append(PresentationState& state, TranscriptRow::Kind kind,
            const std::string& id, std::string text) {
  if (text.empty()) return;
  state.transcriptRows.push_back({kind, id, std::move(text), false, {}});
}
void appendBlocks(PresentationState& state, const Json& content,
                  const std::string& id, bool assistant, bool error) {
  if (content.is_string()) {
    append(state, error ? TranscriptRow::Kind::Error :
                  assistant ? TranscriptRow::Kind::Assistant :
                              TranscriptRow::Kind::User,
           id, content.get<std::string>());
    return;
  }
  if (!content.is_array()) return;
  for (const auto& block : content) {
    const auto type = string(block, "type");
    if (type == "text") {
      append(state, error ? TranscriptRow::Kind::Error :
                    assistant ? TranscriptRow::Kind::Assistant :
                                TranscriptRow::Kind::User,
             id, string(block, "text"));
    } else if (type == "thinking") {
      append(state, TranscriptRow::Kind::Reasoning, id,
             string(block, "thinking"));
    } else if (type == "redacted_thinking") {
      append(state, TranscriptRow::Kind::Reasoning, id,
             "[Reasoning unavailable]");
    } else if (type == "tool_use" || type == "server_tool_use" ||
               type == "mcp_tool_use") {
      auto text = string(block, "name");
      if (text.empty()) text = "Gadget";
      // Name what the Gadget acts on rather than dumping its JSON input.
      if (const auto* input = member(block, "input"); input && input->is_object()) {
        std::string target;
        for (const char* key : {"file_path", "path", "pattern", "command", "url",
                                "query", "description", "skill"}) {
          target = string(*input, key);
          if (!target.empty()) break;
        }
        if (target.empty() && !input->empty()) target = input->dump();
        const auto newline = target.find('\n');
        if (newline != std::string::npos) target = target.substr(0, newline) + " …";
        if (target.size() > 160) {
          std::size_t cut = 159;
          while (cut > 0 && (static_cast<unsigned char>(target[cut]) & 0xc0) == 0x80) --cut;
          target = target.substr(0, cut) + "…";
        }
        if (!target.empty()) text += " " + target;
      }
      append(state, TranscriptRow::Kind::Gadget, id, std::move(text));
    } else if (type == "tool_result" || type == "mcp_tool_result" ||
               type == "web_search_tool_result" ||
               type == "web_fetch_tool_result" ||
               type == "code_execution_tool_result") {
      const auto* result = member(block, "content");
      auto text = result ? contentText(*result) : "";
      if (text.empty()) text = type;
      append(state, TranscriptRow::Kind::GadgetResult, id, std::move(text));
    } else if (!type.empty()) {
      append(state, TranscriptRow::Kind::GadgetResult, id, "[" + type + "]");
    }
  }
}
void appendMessageRows(PresentationState& state, const MessageRef& message) {
  if (!message || !message->is_object()) return;
  const std::weak_ptr<Json> weak = message;
  auto token = state.transcriptMessageTokens.find(weak);
  if (token == state.transcriptMessageTokens.end())
    token = state.transcriptMessageTokens.emplace(
        weak, state.nextTranscriptMessageToken++).first;
  const auto rowStart = state.transcriptRows.size();
  const auto type = string(*message, "type");
  const auto id = string(*message, "uuid");
  if (type == "assistant" || type == "user") {
    const auto* inner = member(*message, "message");
    const auto* content = inner ? member(*inner, "content") : nullptr;
    if (content)
      appendBlocks(state, *content, id, type == "assistant",
                   type == "assistant" &&
                   value::truthy(value::property(*message, "isApiErrorMessage")));
  } else if (type == "progress") {
    const auto* data = member(*message, "data");
    auto label = data ? string(*data, "type") : "";
    auto detail = data ? string(*data, "message") : "";
    if (detail.empty() && data) detail = string(*data, "output");
    if (!detail.empty()) label += ": " + detail;
    append(state, TranscriptRow::Kind::Progress, id, std::move(label));
  } else if (type == "system") {
    auto text = string(*message, "content");
    if (text.empty()) text = string(*message, "subtype");
    append(state, TranscriptRow::Kind::System, id, std::move(text));
  } else if (type == "attachment") {
    const auto* attachment = member(*message, "attachment");
    auto text = attachment ? string(*attachment, "type") : "attachment";
    append(state, TranscriptRow::Kind::System, id, "[" + text + "]");
  }
  for (std::size_t i = rowStart; i < state.transcriptRows.size(); ++i)
    state.transcriptRows[i].scrollbackId = std::to_string(token->second) +
        ":" + std::to_string(i - rowStart);
}
} // namespace

void rebuildTranscriptRows(PresentationState& state) {
  const auto mutations = runtime::messagesMutatedSince(state.transcriptRowsEpoch);
  state.transcriptRowsEpoch = mutations.epoch;
  auto& sources = state.transcriptRowSources;
  auto& rows = state.transcriptRows;
  const bool consistent = sources.empty() ? rows.empty() :
      rows.size() == sources.back().rowEnd;
  std::size_t keep = 0;
  if (consistent && mutations.changed) {
    const auto& changed = *mutations.changed;
    const auto limit = std::min(sources.size(), state.messages.size());
    while (keep < limit) {
      const auto& message = state.messages[keep];
      const auto& source = sources[keep].message;
      if (source.owner_before(message) || message.owner_before(source)) break;
      if (message && std::find(changed.begin(), changed.end(), message.get()) != changed.end())
        break;
      ++keep;
    }
  }
  rows.resize(keep ? sources[keep - 1].rowEnd : 0);
  sources.resize(keep);
  sources.reserve(state.messages.size());
  for (std::size_t i = keep; i < state.messages.size(); ++i) {
    appendMessageRows(state, state.messages[i]);
    sources.push_back({state.messages[i], rows.size()});
  }
  if (state.transcriptMessageTokens.size() > state.messages.size() * 2 + 1024)
    for (auto it = state.transcriptMessageTokens.begin();
         it != state.transcriptMessageTokens.end();) {
      if (it->first.expired()) it = state.transcriptMessageTokens.erase(it);
      else ++it;
    }
}

std::optional<std::string> visibleStreamingText(const PresentationState& state) {
  if (!state.loading || !state.showStreamingText || !state.streamingText ||
      state.streamingText->empty()) return std::nullopt;
  const auto last = state.streamingText->rfind('\n');
  if (last == std::string::npos) return std::nullopt;
  const auto visible = state.streamingText->substr(0, last + 1);
  return visible.empty() ? std::nullopt : std::optional<std::string>{visible};
}
} // namespace batbox::interactive
