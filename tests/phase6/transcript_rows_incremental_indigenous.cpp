#include "interactive/PresentationState.hpp"
#include "interactive/TranscriptRows.hpp"
#include "runtime/MessageMutation.hpp"
#include "runtime/MessageSnapshot.hpp"
#include "terminal_ftxui/PresentationView.hpp"
#include <cassert>
#include <iostream>
#include <random>
#include <string>

namespace ix = batbox::interactive;
using batbox::Json;

namespace {
int serial = 0;
ix::MessageRef make(std::mt19937& random) {
  const auto id = "m" + std::to_string(++serial);
  switch (random() % 6) {
    case 0:
      return std::make_shared<Json>(Json{{"type", "user"}, {"uuid", id},
          {"message", {{"content", "question " + id}}}});
    case 1:
      return std::make_shared<Json>(Json{{"type", "assistant"}, {"uuid", id},
          {"message", {{"content", Json::array({
            Json{{"type", "thinking"}, {"thinking", "reasoning " + id}},
            Json{{"type", "text"}, {"text", "answer " + id + "\nsecond line"}},
            Json{{"type", "tool_use"}, {"id", "t" + id}, {"name", "Read"},
                 {"input", {{"file_path", "/tmp/" + id}}}}})}}}});
    case 2:
      return std::make_shared<Json>(Json{{"type", "user"}, {"uuid", id},
          {"message", {{"content", Json::array({
            Json{{"type", "tool_result"}, {"tool_use_id", "t" + id},
                 {"content", "result body " + id}}})}}}});
    case 3:
      return std::make_shared<Json>(Json{{"type", "progress"}, {"uuid", id},
          {"data", {{"type", "bash_progress"}, {"message", "tick " + id}}}});
    case 4:
      return std::make_shared<Json>(Json{{"type", "system"}, {"uuid", id},
          {"content", "notice " + id}});
    default:
      return std::make_shared<Json>(Json{{"type", "attachment"}, {"uuid", id},
          {"attachment", {{"type", "file"}}}});
  }
}
void expectFullRebuildEquivalent(const ix::PresentationState& state) {
  auto full = state;
  full.transcriptRows.clear();
  full.transcriptRowSources.clear();
  ix::rebuildTranscriptRows(full);
  assert(full.transcriptRows.size() == state.transcriptRows.size());
  for (std::size_t i = 0; i < full.transcriptRows.size(); ++i) {
    const auto& left = full.transcriptRows[i];
    const auto& right = state.transcriptRows[i];
    assert(left.kind == right.kind);
    assert(left.messageId == right.messageId);
    assert(left.text == right.text);
    assert(left.transient == right.transient);
    assert(left.scrollbackId == right.scrollbackId);
  }
}
} // namespace

int main() {
  std::mt19937 random(20261101);
  ix::PresentationState state;
  for (int step = 0; step < 4000; ++step) {
    const auto action = random() % 10;
    if (action < 5 || state.messages.empty()) {
      state.messages.push_back(make(random));
    } else if (action == 5) {
      state.messages.back() = make(random);
    } else if (action == 6) {
      const auto count = random() % state.messages.size();
      state.messages.erase(state.messages.begin(),
                           state.messages.begin() + static_cast<std::ptrdiff_t>(count));
    } else if (action == 7) {
      const auto at = random() % state.messages.size();
      state.messages.erase(state.messages.begin() + static_cast<std::ptrdiff_t>(at));
    } else if (action == 8) {
      const auto at = random() % state.messages.size();
      const auto& target = state.messages[at];
      {
        batbox::runtime::MessageLock lock(target);
        auto& value = lock.value();
        if (value.value("type", "") == "system") value["content"] = "edited " + std::to_string(step);
        else if (value.value("type", "") == "user" && value["message"]["content"].is_string())
          value["message"]["content"] = "edited question " + std::to_string(step);
        else value["uuid"] = "edited-" + std::to_string(step);
      }
    } else {
      state.messages.insert(state.messages.begin() +
          static_cast<std::ptrdiff_t>(random() % state.messages.size()), make(random));
    }
    ix::rebuildTranscriptRows(state);
    expectFullRebuildEquivalent(state);
    if (state.messages.size() > 200)
      state.messages.erase(state.messages.begin(), state.messages.begin() + 100);
  }

  // A rebuild over an unchanged message list keeps every existing row.
  ix::PresentationState stable;
  stable.messages.push_back(std::make_shared<Json>(Json{{"type", "user"}, {"uuid", "long"},
      {"message", {{"content", std::string(200, 'x') + " heap allocated row text"}}}}));
  for (int i = 0; i < 50; ++i) stable.messages.push_back(make(random));
  ix::rebuildTranscriptRows(stable);
  const auto* firstText = stable.transcriptRows.front().text.data();
  ix::rebuildTranscriptRows(stable);
  assert(stable.transcriptRows.front().text.data() == firstText);
  stable.messages.push_back(make(random));
  ix::rebuildTranscriptRows(stable);
  assert(stable.transcriptRows.front().text.data() == firstText);
  expectFullRebuildEquivalent(stable);

  // Rows edited directly (not through rebuild) force a full rebuild.
  stable.transcriptRows.clear();
  ix::rebuildTranscriptRows(stable);
  expectFullRebuildEquivalent(stable);
  assert(!stable.transcriptRows.empty());

  // The plain --print drain emits each message once, in order.
  ix::PresentationState printed;
  batbox::terminal_ftxui::PlainCursor cursor;
  std::string expected, drained;
  for (int i = 0; i < 3000; ++i) {
    const auto text = "line " + std::to_string(i);
    printed.messages.push_back(std::make_shared<Json>(Json{{"type", "system"},
        {"uuid", "p" + std::to_string(i)}, {"content", text}}));
    expected += text + "\n";
    if (i % 3 == 0) drained += batbox::terminal_ftxui::drainPresentationPlain(printed, cursor);
  }
  drained += batbox::terminal_ftxui::drainPresentationPlain(printed, cursor);
  assert(drained == expected);
  assert(batbox::terminal_ftxui::drainPresentationPlain(printed, cursor).empty());
  assert(cursor.emitted.size() == 3000 && cursor.emittedIds.size() == 3000);

  std::cout << "incremental transcript rows match a full rebuild\n";
  return 0;
}
