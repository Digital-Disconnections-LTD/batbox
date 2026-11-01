#include "interactive/PresentationState.hpp"
#include "interactive/TranscriptRows.hpp"
#include "messages/Context.hpp"
#include "flights/Transcript.hpp"
#include <algorithm>
#include <stdexcept>

namespace batbox::interactive {
namespace {
const Json& field(const Json& value, const char* key) {
  if (!value.is_object()) throw std::runtime_error("Presentation message is not an object");
  const auto found = value.find(key);
  if (found == value.end())
    throw std::runtime_error(std::string("Presentation message missing ") + key);
  return *found;
}
void require(const auto& callback, const char* name) {
  if (!callback) throw std::logic_error(std::string("Presentation import required: ") + name);
}
void appendMessage(PresentationState& state, const MessageRef& message,
                   const PresentationImports& imports) {
  if (messages::isCompactBoundaryMessage(*message)) {
    if (imports.fullscreen) {
      auto last = state.messages.end();
      for (auto it = state.messages.end(); it != state.messages.begin();) {
        --it;
        if (*it && messages::isCompactBoundaryMessage(**it)) {last = it; break;}
      }
      if (last != state.messages.end()) state.messages.erase(state.messages.begin(), last);
    } else state.messages.clear();
    state.messages.push_back(message);
    require(imports.newConversationId, "newConversationId");
    state.conversationId = imports.newConversationId();
    if (imports.proactive || imports.kairos) {
      state.contextBlocked = false;
      if (imports.setContextBlocked) imports.setContextBlocked(false);
    }
  } else if (value::property(*message, "type") == "progress" &&
             flights::isEphemeralGadgetProgress(
                 value::property(field(*message, "data"), "type"),
                 imports.proactive, imports.kairos)) {
    if (!state.messages.empty()) {
      const auto& last = state.messages.back();
      if (last && value::property(*last, "type") == "progress" &&
          value::property(*last, "parentToolUseID") ==
              value::property(*message, "parentToolUseID") &&
          value::property(field(*last, "data"), "type") ==
              value::property(field(*message, "data"), "type")) {
        state.messages.back() = message;
      } else state.messages.push_back(message);
    } else state.messages.push_back(message);
  } else state.messages.push_back(message);

  if (imports.proactive || imports.kairos) {
    if (value::property(*message, "type") == "assistant") {
      const auto blocked = value::truthy(value::property(*message, "isApiErrorMessage"));
      state.contextBlocked = blocked;
      if (imports.setContextBlocked) imports.setContextBlocked(blocked);
    }
  }
  rebuildTranscriptRows(state);
}
} // namespace

void applyStreamMessage(PresentationState& state, const StreamInput& input,
                        const PresentationImports& imports) {
  if (!input.event) throw std::invalid_argument("Stream event reference required");
  StreamCallbacks callbacks;
  callbacks.onMessage = [&](const Json&) {
    appendMessage(state, input.event, imports);
  };
  callbacks.onUpdateLength = [&](const std::string& text) {
    state.responseLengthUtf16 += value::stringLength(text);
  };
  callbacks.onSetStreamMode = [&](const std::string& mode) {
    state.streamMode = mode;
  };
  callbacks.onStreamingGadgetUses = [&](GadgetUsesUpdate update) {
    state.streamingGadgetUses = update(state.streamingGadgetUses);
  };
  callbacks.onTombstone = [&](const Json& target) {
    if (!input.tombstoneTarget)
      throw std::logic_error("Tombstone target identity required");
    std::erase_if(state.messages, [&](const MessageRef& message) {
      return message == input.tombstoneTarget;
    });
    require(imports.removeTranscriptMessage, "removeTranscriptMessage");
    imports.removeTranscriptMessage(field(target, "uuid"));
    rebuildTranscriptRows(state);
  };
  callbacks.onStreamingThinking = [&](ThinkingUpdate update) {
    state.streamingThinking = update(state.streamingThinking);
  };
  callbacks.onApiMetrics = [&](double ttftMs) {
    require(imports.stream.nowMs, "nowMs");
    const double now = imports.stream.nowMs();
    const auto baseline = state.responseLengthUtf16;
    state.apiMetrics.push_back({ttftMs, now, now, baseline, baseline});
  };
  callbacks.onStreamingText = [&](TextUpdate update) {
    if (state.showStreamingText)
      state.streamingText = update(state.streamingText);
  };
  callbacks.appendStreamingText = [&](const std::string& text) {
    if (!state.showStreamingText) return;
    if (!state.streamingText) state.streamingText.emplace();
    state.streamingText->append(text);
  };
  callbacks.appendGadgetInput = [&](const Json& index, const std::string& fragment) {
    auto& uses = state.streamingGadgetUses;
    const auto found = std::find_if(uses.begin(), uses.end(),
        [&](const StreamingGadgetUse& use) { return use.index == index; });
    if (found == uses.end()) return;
    found->unparsedGadgetInput.append(fragment);
    // The updater path re-appends the extended use; keep that order.
    std::rotate(found, found + 1, uses.end());
  };
  handleMessageFromStream(*input.event, callbacks, imports.stream);
  ++state.revision;
}

void updateViewport(PresentationState& state, int columns, int rows) {
  if (columns <= 0 || rows <= 0)
    throw std::invalid_argument("Positive terminal viewport required");
  state.viewport.columns = columns;
  state.viewport.rows = rows;
  ++state.revision;
}

void scrollViewport(PresentationState& state, std::ptrdiff_t deltaRows,
                    std::size_t renderedRowCount, std::size_t bodyHeight) {
  const auto maxTop = renderedRowCount > bodyHeight ?
      renderedRowCount - bodyHeight : 0;
  const auto current = state.viewport.follow ? maxTop :
      std::min(state.viewport.topRow, maxTop);
  if (deltaRows < 0) {
    const auto backwards = static_cast<std::size_t>(-(deltaRows + 1)) + 1;
    state.viewport.topRow = backwards > current ? 0 : current - backwards;
  } else {
    const auto forwards = static_cast<std::size_t>(deltaRows);
    state.viewport.topRow = forwards > maxTop - current ?
        maxTop : current + forwards;
  }
  state.viewport.follow = state.viewport.topRow == maxTop;
  ++state.revision;
}
} // namespace batbox::interactive
