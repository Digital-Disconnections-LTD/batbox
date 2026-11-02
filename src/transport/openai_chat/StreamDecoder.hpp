#pragma once
#include "runtime/JsonValue.hpp"
#include "transport/SSE.hpp"
#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace batbox::transport::openai_chat {

// Chat Completions SSE (data-only frames, [DONE]) -> Anthropic stream events:
// message_start, content_block_start/delta/stop with sequential indices,
// message_delta with stop_reason and usage, message_stop. Tool arguments
// stream as input_json_delta fragments. A frame carrying an error object
// throws APIError with an Anthropic error body; an unparsable frame, a tool
// call that never gets a name, or arguments that arrive for a tool whose
// block already closed throw std::runtime_error, so the caller can fall back
// to a non-streaming request.
class StreamDecoder {
public:
  // model is echoed in message_start; requestBytes sizes the input estimate
  // used when the server never reports usage.
  StreamDecoder(std::string model, std::size_t requestBytes, std::string baseUrl = {});
  std::vector<Json> consume(const ServerSentEvent& event);
  std::vector<Json> consumeData(const std::string& data);
  // End of the HTTP body. Completes the message after finish_reason or
  // [DONE]; throws when the stream stopped before either.
  std::vector<Json> finish();
  bool complete() const { return complete_; }
private:
  enum class Block { None, Text, Thinking, Tool };
  struct Tool {
    std::string serverId, name, pending;
    std::optional<std::size_t> index;
    bool closed = false;
  };
  void start(std::vector<Json>& out);
  void open(std::vector<Json>& out, Block kind, Json contentBlock);
  void close(std::vector<Json>& out);
  void toolDelta(std::vector<Json>& out, const Json& call);
  void startTool(std::vector<Json>& out, Tool& tool);
  void end(std::vector<Json>& out);
  std::string model_;
  std::size_t requestBytes_ = 0;
  std::string baseUrl_;
  bool started_ = false, finished_ = false, complete_ = false, sawToolCall_ = false;
  std::size_t nextIndex_ = 0, generatedBytes_ = 0;
  Block openKind_ = Block::None;
  std::size_t openIndex_ = 0;
  std::optional<std::string> openToolKey_;
  std::map<std::string, Tool> tools_;
  std::vector<std::string> toolOrder_;
  std::set<std::string> toolIds_;
  std::string finishReason_;
  Json usage_ = nullptr;
};

} // namespace batbox::transport::openai_chat
