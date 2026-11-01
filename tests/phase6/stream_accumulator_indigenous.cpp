#include "interactive/PresentationState.hpp"
#include "interactive/StreamReducer.hpp"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

namespace ix = batbox::interactive;
using batbox::Json;

namespace {
ix::MessageRef event(Json body) {
  return std::make_shared<Json>(Json{{"type", "stream_event"}, {"event", std::move(body)}});
}
ix::MessageRef textDelta(const std::string& text) {
  return event({{"type", "content_block_delta"}, {"index", 0},
                {"delta", {{"type", "text_delta"}, {"text", text}}}});
}
ix::MessageRef toolStart(int index, const std::string& id, const std::string& name) {
  return event({{"type", "content_block_start"}, {"index", index},
                {"content_block", {{"type", "tool_use"}, {"id", id},
                                   {"name", name}, {"input", Json::object()}}}});
}
ix::MessageRef inputDelta(int index, const std::string& fragment) {
  return event({{"type", "content_block_delta"}, {"index", index},
                {"delta", {{"type", "input_json_delta"}, {"partial_json", fragment}}}});
}
ix::PresentationImports imports() {
  ix::PresentationImports result;
  result.stream.connectorTextEnabled = [] { return false; };
  result.stream.nowMs = [] { return 1000.0; };
  result.newConversationId = [] { return "conversation"; };
  result.removeTranscriptMessage = [](const Json&) {};
  return result;
}
// Replays events through only the updater callbacks, the path Bat streams
// still use, so the in-place presentation path can be compared against it.
struct UpdaterReplay {
  std::vector<ix::StreamingGadgetUse> uses;
  std::optional<std::string> text;
  void apply(const ix::MessageRef& message) {
    ix::StreamCallbacks callbacks;
    callbacks.onMessage = [](const Json&) {};
    callbacks.onUpdateLength = [](const std::string&) {};
    callbacks.onSetStreamMode = [](const std::string&) {};
    callbacks.onStreamingGadgetUses = [this](ix::GadgetUsesUpdate update) {
      uses = update(uses);
    };
    callbacks.onStreamingText = [this](ix::TextUpdate update) { text = update(text); };
    ix::StreamImports streamImports;
    streamImports.connectorTextEnabled = [] { return false; };
    streamImports.nowMs = [] { return 1000.0; };
    ix::handleMessageFromStream(*message, callbacks, streamImports);
  }
};
bool sameUses(const std::vector<ix::StreamingGadgetUse>& left,
              const std::vector<ix::StreamingGadgetUse>& right) {
  if (left.size() != right.size()) return false;
  for (std::size_t i = 0; i < left.size(); ++i)
    if (left[i].index != right[i].index || left[i].contentBlock != right[i].contentBlock ||
        left[i].unparsedGadgetInput != right[i].unparsedGadgetInput) return false;
  return true;
}
} // namespace

int main() {
  const auto bindings = imports();

  {
    ix::PresentationState state;
    UpdaterReplay replay;
    std::string expected;
    for (int i = 0; i < 6000; ++i) {
      const auto piece = "chunk " + std::to_string(i) + (i % 37 == 0 ? "\n" : " ");
      expected += piece;
      const auto message = textDelta(piece);
      ix::applyStreamMessage(state, {message, {}}, bindings);
      replay.apply(message);
    }
    assert(state.streamingText && *state.streamingText == expected);
    assert(replay.text == state.streamingText);
    assert(state.responseLengthUtf16 == expected.size());
    const auto restart = event({{"type", "content_block_start"}, {"index", 1},
        {"content_block", {{"type", "text"}, {"text", ""}}}});
    ix::applyStreamMessage(state, {restart, {}}, bindings);
    assert(!state.streamingText);
    ix::applyStreamMessage(state, {textDelta("again"), {}}, bindings);
    assert(state.streamingText == std::optional<std::string>("again"));
  }

  {
    ix::PresentationState state;
    state.showStreamingText = false;
    ix::applyStreamMessage(state, {textDelta("hidden"), {}}, bindings);
    assert(!state.streamingText);
    assert(state.responseLengthUtf16 == 6);
  }

  {
    std::string content;
    for (int line = 0; content.size() < 100 * 1024; ++line)
      content += "line " + std::to_string(line) + " \"quoted\" \\ tab\t unicode é 🙂\n";
    const Json input = {{"file_path", "/tmp/out.txt"}, {"content", content}};
    // The service streams JSON text; escape non-ASCII so fragments stay whole.
    const auto payload = input.dump(-1, ' ', true);
    const std::string small = R"({"command":"ls -la"})";

    ix::PresentationState state;
    UpdaterReplay replay;
    const auto feed = [&](const ix::MessageRef& message) {
      ix::applyStreamMessage(state, {message, {}}, bindings);
      replay.apply(message);
    };
    feed(toolStart(0, "write-1", "Write"));
    feed(toolStart(1, "bash-1", "Bash"));
    std::size_t smallAt = 0;
    for (std::size_t at = 0; at < payload.size(); at += 20) {
      feed(inputDelta(0, payload.substr(at, 20)));
      if (at % 400 == 0 && smallAt < small.size()) {
        feed(inputDelta(1, small.substr(smallAt, 3)));
        smallAt += 3;
      }
      if (at % 10000 == 0) feed(inputDelta(7, "{ignored"));
      assert(sameUses(state.streamingGadgetUses, replay.uses));
    }
    while (smallAt < small.size()) {
      feed(inputDelta(1, small.substr(smallAt, 3)));
      smallAt += 3;
    }
    assert(state.streamingGadgetUses.size() == 2);
    assert(sameUses(state.streamingGadgetUses, replay.uses));
    const ix::StreamingGadgetUse* write = nullptr;
    const ix::StreamingGadgetUse* bash = nullptr;
    for (const auto& use : state.streamingGadgetUses) {
      if (use.index == 0) write = &use;
      if (use.index == 1) bash = &use;
    }
    assert(write && bash);
    assert(write->unparsedGadgetInput == payload);
    assert(Json::parse(write->unparsedGadgetInput) == input);
    assert(bash->unparsedGadgetInput == small);
    assert(write->contentBlock.at("name") == "Write");
    // The most recently extended Gadget use moves to the end, as before.
    assert(state.streamingGadgetUses.back().index == 0);

    feed(event({{"type", "message_stop"}}));
    assert(state.streamingGadgetUses.empty());
    assert(replay.uses.empty());
  }

  std::cout << "stream accumulators append in place and match the updater path\n";
  return 0;
}
