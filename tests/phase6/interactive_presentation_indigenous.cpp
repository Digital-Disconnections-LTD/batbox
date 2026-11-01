#include "interactive/PresentationState.hpp"
#include "interactive/TranscriptRows.hpp"
#include "terminal_ftxui/PresentationView.hpp"
#include "ftxui/screen/screen.hpp"
#include <cassert>
#include <iostream>

namespace ix = batbox::interactive;
namespace tf = batbox::terminal_ftxui;
using batbox::Json;

static ix::MessageRef ref(Json value) {
  return std::make_shared<Json>(std::move(value));
}

int main() {
  ix::PresentationState state;
  state.loading = true;
  std::string removed;
  ix::PresentationImports imports;
  imports.stream.connectorTextEnabled = [] { return false; };
  imports.stream.nowMs = [] { return 4000.0; };
  imports.newConversationId = [] { return "next-conversation"; };
  imports.removeTranscriptMessage = [&](const Json& uuid) {
    removed = uuid.get<std::string>();
  };

  auto textDelta = ref(Json{{"type", "stream_event"},
    {"event", {{"type", "content_block_delta"}, {"index", 0},
      {"delta", {{"type", "text_delta"}, {"text", "🙂\n"}}}}}});
  ix::applyStreamMessage(state, {textDelta, {}}, imports);
  assert(state.responseLengthUtf16 == 3);
  assert(state.streamingText == "🙂\n");
  assert(ix::visibleStreamingText(state) == "🙂\n");
  auto final = ref(Json{{"type", "assistant"}, {"uuid", "a1"},
    {"message", {{"content", Json::array({
      Json{{"type", "text"}, {"text", "🙂"}}})}}}});
  ix::applyStreamMessage(state, {final, {}}, imports);
  assert(!state.streamingText);
  assert(state.messages.size() == 1 && state.messages.front() == final);
  assert(state.transcriptRows.size() == 1 &&
         state.transcriptRows.front().text == "🙂");

  auto tick1 = ref(Json{{"type", "progress"}, {"parentToolUseID", "t"},
    {"data", {{"type", "bash_progress"}, {"message", "one"}}}});
  auto tick2 = ref(Json{{"type", "progress"}, {"parentToolUseID", "t"},
    {"data", {{"type", "bash_progress"}, {"message", "two"}}}});
  ix::applyStreamMessage(state, {tick1, {}}, imports);
  ix::applyStreamMessage(state, {tick2, {}}, imports);
  assert(state.messages.size() == 2 && state.messages.back() == tick2);
  auto tombstone = ref(Json{{"type", "tombstone"}, {"message", *final}});
  ix::applyStreamMessage(state, {tombstone, final}, imports);
  assert(state.messages.size() == 1 && state.messages.front() == tick2);
  assert(removed == "a1");

  auto boundary = ref(Json{{"type", "system"},
    {"subtype", "compact_boundary"}, {"content", "Compacted"},
    {"uuid", "b1"}});
  ix::applyStreamMessage(state, {boundary, {}}, imports);
  assert(state.messages.size() == 1 && state.messages.front() == boundary);
  assert(state.conversationId == "next-conversation");

  state.loading = false;
  state.transcriptRows.clear();
  for (int i = 0; i < 10; ++i)
    state.transcriptRows.push_back({ix::TranscriptRow::Kind::Assistant,
      std::to_string(i), "row" + std::to_string(i), false});
  state.input.text = "abc界";
  state.input.caretUtf16 = 4;
  ix::updateViewport(state, 12, 6);
  auto layout = tf::layoutPresentation(state, 12, 6);
  assert(layout.transcriptRowCount == 10);
  assert(layout.bodyHeight == 5 && layout.topRow == 5);
  ftxui::Screen drawn(12, 6);
  ftxui::Render(drawn, tf::renderPresentation(state, {12, 6}));
  assert(drawn.CellAt(2, 0).character == "r");
  assert(drawn.CellAt(0, 5).character == "❯");
  assert(drawn.cursor().x == 7 && drawn.cursor().y == 5);
  assert(drawn.cursor().shape == ftxui::Screen::Cursor::Bar);

  ix::scrollViewport(state, -2, layout.transcriptRowCount, layout.bodyHeight);
  assert(!state.viewport.follow && state.viewport.topRow == 3);
  ftxui::Screen scrolled(12, 6);
  ftxui::Render(scrolled, tf::renderPresentation(state, {12, 6}));
  assert(scrolled.CellAt(5, 0).character == "3");
  ix::scrollViewport(state, 100, layout.transcriptRowCount, layout.bodyHeight);
  assert(state.viewport.follow && state.viewport.topRow == 5);

  state.modal = ix::UiRequest{ix::UiRequest::Kind::Select,
    "select", "Choose", "Which?", {"One", "Two"}, 1, true, false};
  ftxui::Screen modal(12, 6);
  ftxui::Render(modal, tf::renderPresentation(state, {12, 6}));
  assert(modal.cursor().shape == ftxui::Screen::Cursor::Hidden);
  assert(modal.CellAt(0, 5).inverted);

  tf::PlainCursor plain;
  state.messages = {boundary};
  assert(tf::drainPresentationPlain(state, plain) == "Compacted\n");
  assert(tf::drainPresentationPlain(state, plain).empty());
  std::cout << "14 native presentation reducer, viewport, caret and modal checks pass\n";
}
