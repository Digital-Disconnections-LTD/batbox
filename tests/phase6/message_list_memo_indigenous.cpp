#include "interactive/MessageListMemo.hpp"
#include "runtime/MessageMutation.hpp"
#include "runtime/MessageSnapshot.hpp"
#include "streaming/Reducer.hpp"
#include <cassert>
#include <iostream>

using batbox::Json;
namespace ix = batbox::interactive;

namespace {
ix::MessageRef message(Json data) { return std::make_shared<Json>(std::move(data)); }
ix::MessageListSettings settings(batbox::messages::Services& services) {
  ix::MessageListSettings s;
  s.gadgets = std::make_shared<ix::GroupingGadgetSet>();
  s.gadgets->gadgets.push_back({"Read", false});
  s.backgroundProtocol = {"task-notification", "status", "summary", "Background command "};
  s.visibility = {"NO_CONTENT", "INTERRUPT", [](const std::string&) { return false; }};
  auto& cls = s.readSearch.classification;
  cls.feature = [](const std::string&) { return false; };
  cls.isFullscreen = [] { return true; };
  cls.isAutoManagedMemoryFile = [](const std::string&) { return false; };
  cls.isMemoryDirectory = [](const std::string&) { return false; };
  cls.isAutoManagedMemoryPattern = [](const std::string&) { return false; };
  cls.isShellCommandTargetingMemory = [](const std::string&) { return false; };
  cls.getReplPrimitiveGadgets = [] { return batbox::gadgets::Gadgets{}; };
  cls.replGadgetName = "REPL";
  cls.writeGadgetName = "Write";
  cls.editGadgetName = "Edit";
  cls.gadgetSearchName = "ToolSearch";
  cls.bashGadgetName = "Bash";
  s.readSearch.extractBashCommentLabel = [](const std::string&) -> std::optional<std::string> {
    return std::nullopt;
  };
  s.readSearch.getDisplayPath = [](const std::string& path) { return path; };
  s.readSearch.isTeamMemoryWriteOrEdit = [](const std::string&, const Json&) { return false; };
  s.readSearch.isTeamMemorySearch = [](const Json&) { return false; };
  s.readSearch.isTeamMemFile = [](const std::string&) { return false; };
  s.readSearch.detectGitOperation = [](const std::string&, const std::string&) { return Json(); };
  s.messageServices = &services;
  s.fullscreen = true;
  s.virtualScroll = true;
  return s;
}
ix::MessageRef user(const std::string& id, const std::string& text) {
  return message({{"type", "user"}, {"uuid", id}, {"timestamp", "t"},
                  {"message", {{"role", "user"}, {"content", text}}}});
}
ix::MessageRef assistant(const std::string& id, Json content) {
  return message({{"type", "assistant"}, {"uuid", id}, {"timestamp", "t"},
                  {"message", {{"id", "api-" + id}, {"role", "assistant"},
                               {"content", std::move(content)}}}});
}
ix::MessageRef toolResult(const std::string& id, const std::string& toolId) {
  return message({{"type", "user"}, {"uuid", id}, {"timestamp", "t"},
                  {"message", {{"role", "user"}, {"content", Json::array({
                    Json{{"type", "tool_result"}, {"tool_use_id", toolId},
                         {"content", "result for " + toolId}}})}}}});
}
std::string fingerprint(const ix::PreparedMessageList& list) {
  Json out = Json::object();
  out["normalized"] = Json::array();
  for (const auto& m : list.normalizedMessages) out["normalized"].push_back(m ? *m : Json());
  out["renderable"] = Json::array();
  for (const auto& item : *list.renderable)
    out["renderable"].push_back(ix::materializeRenderItem(item));
  out["hidden"] = list.hiddenMessageCount;
  out["truncated"] = list.hasTruncatedMessages;
  return out.dump();
}
std::string expected(const ix::MessageRefs& messages, const ix::MessageListSettings& s) {
  std::optional<ix::SliceAnchor> anchor;
  return fingerprint(ix::prepareMessageList(messages, s, anchor));
}
std::shared_ptr<ix::PreparedMessageList> rebuiltBeforeClear(ix::MessageListMemo& memo,
    const ix::MessageRefs& messages, const ix::MessageListSettings& s,
    std::optional<ix::SliceAnchor>& anchor) {
  return memo.prepare(messages, s, anchor);
}
} // namespace

int main() {
  batbox::messages::Services services;
  // Constant so that memoized and fresh preparations generate identical
  // synthetic messages regardless of how often each calls the service.
  services.randomUUID = [] { return std::string("00000000-0000-4000-8000-000000000001"); };
  services.nowISO = [] { return "2026-11-01T00:00:00.000Z"; };
  auto s = settings(services);

  ix::MessageRefs messages;
  for (int i = 0; i < 12; ++i) {
    const auto n = std::to_string(i);
    messages.push_back(user("u" + n, "question " + n));
    messages.push_back(assistant("a" + n, Json::array({
      Json{{"type", "text"}, {"text", "answer " + n}},
      Json{{"type", "tool_use"}, {"id", "tool" + n}, {"name", "Read"},
           {"input", {{"file_path", "/tmp/" + n}}}}})));
    messages.push_back(toolResult("r" + n, "tool" + n));
  }
  messages.push_back(message({{"type", "progress"}, {"uuid", "p"},
                              {"data", {{"type", "bash_progress"}}}}));

  ix::MessageListMemo memo;
  std::optional<ix::SliceAnchor> anchor;
  auto first = memo.prepare(messages, s, anchor);
  assert(fingerprint(*first) == expected(messages, s));
  assert(memo.normalizations() == messages.size());
  assert(memo.preparations() == 1);

  // Identical inputs reuse the whole prepared list.
  auto again = memo.prepare(messages, settings(services), anchor);
  assert(again == first);
  assert(memo.preparations() == 1 && memo.normalizations() == messages.size());

  // An appended message normalizes only that message.
  messages.push_back(user("tail", "one more"));
  auto appended = memo.prepare(messages, s, anchor);
  assert(appended != first);
  assert(memo.normalizations() == messages.size());
  assert(fingerprint(*appended) == expected(messages, s));

  // A message edited in place after it was published updates the view.
  {
    auto target = messages[4];
    {
      batbox::runtime::MessageLock lock(target);
      lock.value()["message"]["content"][0]["text"] = "edited answer";
    }
  }
  const auto beforeEdit = memo.normalizations();
  auto edited = memo.prepare(messages, s, anchor);
  assert(memo.normalizations() == beforeEdit + 1);
  assert(fingerprint(*edited) == expected(messages, s));
  assert(fingerprint(*edited).find("edited answer") != std::string::npos);

  // The stream reducer yields an assistant message, then message_delta edits
  // it in place. The prepared list must pick up the new stop reason.
  {
    batbox::streaming::ReducerConfig config;
    config.model = "model";
    config.resolvedModel = "model";
    config.nowMillis = [] { return 10; };
    config.randomUUID = [] { return "streamed-1"; };
    config.timestamp = [] { return "now"; };
    config.calculateUSDCost = [](const std::string&, const Json&) { return 0.; };
    config.addToTotalFlightCost = [](double, const Json&, const std::string&) { return 0.; };
    config.getErrorMessageIfRefusal = [](const Json&, const std::string&) -> std::optional<Json> {
      return std::nullopt;
    };
    config.createAssistantAPIErrorMessage = [](const std::string&, const std::string&,
                                               const std::string&) { return Json::object(); };
    batbox::streaming::Reducer reducer(config);
    reducer.consume(Json{{"type", "message_start"}, {"message", {{"content", Json::array()},
        {"usage", {{"input_tokens", 1}, {"output_tokens", 0}}}}}});
    reducer.consume(Json{{"type", "content_block_start"}, {"index", 0},
        {"content_block", {{"type", "text"}, {"text", ""}}}});
    reducer.consume(Json{{"type", "content_block_delta"}, {"index", 0},
        {"delta", {{"type", "text_delta"}, {"text", "streamed reply"}}}});
    const auto yielded = reducer.consume(Json{{"type", "content_block_stop"}, {"index", 0}});
    assert(!yielded.empty() && yielded.front() == reducer.messages().front());
    assert(yielded.front()->at("type") == "assistant");
    messages.push_back(yielded.front());
    auto streamed = memo.prepare(messages, s, anchor);
    assert(fingerprint(*streamed) == expected(messages, s));
    assert(fingerprint(*streamed).find("end_turn") == std::string::npos);
    reducer.consume(Json{{"type", "message_delta"}, {"usage", {{"output_tokens", 7}}},
        {"delta", {{"stop_reason", "end_turn"}}}});
    auto finished = memo.prepare(messages, s, anchor);
    assert(finished != streamed);
    assert(fingerprint(*finished) == expected(messages, s));
    assert(fingerprint(*finished).find("end_turn") != std::string::npos);
  }

  // A multi-block message earlier in the list changes how later messages
  // derive their normalized identifiers; the memo follows that chain state.
  const auto beforeChain = memo.normalizations();
  messages.insert(messages.begin(), assistant("head", Json::array({
    Json{{"type", "text"}, {"text", "two"}}, Json{{"type", "text"}, {"text", "blocks"}}})));
  auto chained = memo.prepare(messages, s, anchor);
  assert(fingerprint(*chained) == expected(messages, s));
  // The head and the first turn (whose chain state flipped) renormalize.
  assert(memo.normalizations() == beforeChain + 3);
  messages.erase(messages.begin(), messages.begin() + 2);
  auto unchained = memo.prepare(messages, s, anchor);
  assert(fingerprint(*unchained) == expected(messages, s));

  // Streaming Gadget input and mode changes rebuild the list without
  // renormalizing messages.
  const auto normalizedBefore = memo.normalizations();
  auto streaming = settings(services);
  streaming.streamingGadgetUses.push_back({Json(0), Json{{"type", "tool_use"},
      {"id", "live"}, {"name", "Bash"}, {"input", Json::object()}}, "{\"comm"});
  auto live = memo.prepare(messages, streaming, anchor);
  assert(fingerprint(*live) == expected(messages, streaming));
  // Input fragments do not change what the list shows for a streaming use.
  streaming.streamingGadgetUses.back().unparsedGadgetInput += "and\":\"ls\"}";
  auto live2 = memo.prepare(messages, streaming, anchor);
  assert(live2 == live);
  assert(fingerprint(*live2) == expected(messages, streaming));
  streaming.streamingGadgetUses.push_back({Json(1), Json{{"type", "tool_use"},
      {"id", "live-2"}, {"name", "Read"}, {"input", Json::object()}}, ""});
  auto live3 = memo.prepare(messages, streaming, anchor);
  assert(live3 != live2);
  assert(fingerprint(*live3) == expected(messages, streaming));
  auto transcript = settings(services);
  transcript.transcriptMode = true;
  auto shown = memo.prepare(messages, transcript, anchor);
  assert(fingerprint(*shown) == expected(messages, transcript));
  assert(memo.normalizations() == normalizedBefore);

  // Capped lists keep their slice anchor semantics and are never reused.
  auto capped = settings(services);
  capped.virtualScroll = false;
  std::optional<ix::SliceAnchor> memoAnchor, freshAnchor;
  const auto preparationsBefore = memo.preparations();
  auto c1 = memo.prepare(messages, capped, memoAnchor);
  auto c2 = memo.prepare(messages, capped, memoAnchor);
  assert(c1 != c2 && memo.preparations() == preparationsBefore + 2);
  auto direct = ix::prepareMessageList(messages, capped, freshAnchor);
  assert(fingerprint(*c2) == fingerprint(direct));

  // Row planning reads plain items through the shared message and groups
  // through a materialized copy equal to materializeRenderItem.
  for (const auto& item : *rebuiltBeforeClear(memo, messages, s, anchor)->renderable) {
    const auto view = ix::materializedView(item);
    if (item.members.empty() && !item.collapsedItems) assert(view == item.message);
    assert(*view == ix::materializeRenderItem(item));
  }
  const ix::RenderItem group(message({{"type", "grouped_tool_use"}, {"uuid", "g"},
      {"toolName", "Read"}}), {messages[1], messages[4]}, {messages[2], messages[5]},
      messages[1]);
  const auto groupView = ix::materializedView(group);
  assert(groupView != group.message && *groupView == ix::materializeRenderItem(group));
  assert(groupView->at("messages").size() == 2);

  memo.clear();
  auto rebuilt = memo.prepare(messages, s, anchor);
  assert(fingerprint(*rebuilt) == expected(messages, s));

  std::cout << "message list memo reuses unchanged work and follows in-place edits\n";
  return 0;
}
