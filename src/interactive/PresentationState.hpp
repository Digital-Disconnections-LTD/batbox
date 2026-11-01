#pragma once

#include "interactive/CommandUi.hpp"
#include "interactive/StreamReducer.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace batbox::interactive {

using MessageRef = std::shared_ptr<Json>;

struct TranscriptRow {
  enum class Kind {User, Assistant, Reasoning, Gadget, GadgetResult,
                   Progress, System, Error};
  Kind kind = Kind::System;
  std::string messageId;
  std::string text;
  bool transient = false;
  // Stable across list rebuilds even when earlier rows are removed.
  std::string scrollbackId;
};

// Where one message's transcript rows end, as of the last rebuild.
struct TranscriptRowSource {
  std::weak_ptr<Json> message;
  std::size_t rowEnd = 0;
};

struct InputState {
  std::string text;
  Json pastedContents=Json::object();
  // The controller keeps this in JavaScript UTF-16 code units. Rendering
  // converts it to a terminal column after grapheme layout.
  std::size_t caretUtf16 = 0;
  bool focused = true;
  std::string poetry = "❯ ";
};

struct Viewport {
  int columns = 80;
  int rows = 24;
  std::size_t topRow = 0;
  bool follow = true;
};

struct ApiMetric {
  double ttftMs = 0;
  double firstTokenTime = 0;
  double lastTokenTime = 0;
  std::size_t responseLengthBaseline = 0;
  std::size_t endResponseLength = 0;
};

struct PresentationState {
  std::vector<MessageRef> messages;
  std::vector<TranscriptRow> transcriptRows;
  std::map<std::weak_ptr<Json>, std::uint64_t,
           std::owner_less<std::weak_ptr<Json>>> transcriptMessageTokens;
  std::uint64_t nextTranscriptMessageToken = 1;
  // Parallel to `messages` at the last rebuild; rebuilds keep the rows of
  // the unchanged leading messages and redo only the rest.
  std::vector<TranscriptRowSource> transcriptRowSources;
  std::uint64_t transcriptRowsEpoch = 0;
  std::vector<StreamingGadgetUse> streamingGadgetUses;
  std::optional<StreamingThinking> streamingThinking;
  std::optional<std::string> streamingText;
  std::optional<std::string> streamMode;
  std::size_t responseLengthUtf16 = 0;
  std::vector<ApiMetric> apiMetrics;
  std::optional<UiRequest> modal;
  InputState input;
  Viewport viewport;
  std::string conversationId;
  bool loading = false;
  bool showStreamingText = true;
  bool contextBlocked = false;
  std::uint64_t revision = 0;
};

struct StreamInput {
  MessageRef event;
  // Indigenous query must retain the same message pointer when it emits a
  // tombstone; serializing the nested message loses object identity.
  MessageRef tombstoneTarget;
};

struct PresentationImports {
  StreamImports stream;
  bool fullscreen = false;
  bool proactive = false;
  bool kairos = false;
  std::function<std::string()> newConversationId;
  std::function<void(bool)> setContextBlocked;
  std::function<void(const Json&)> removeTranscriptMessage;
};

void applyStreamMessage(PresentationState& state, const StreamInput& input,
                        const PresentationImports& imports);
void updateViewport(PresentationState& state, int columns, int rows);
void scrollViewport(PresentationState& state, std::ptrdiff_t deltaRows,
                    std::size_t renderedRowCount, std::size_t bodyHeight);

} // namespace batbox::interactive
