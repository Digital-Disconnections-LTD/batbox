#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
namespace batbox::transport {
struct Newline { std::size_t preceding, index; bool carriage; };
std::optional<Newline> findNewlineIndex(std::string_view buffer, std::optional<std::size_t> start);
class LineDecoder {
  std::string buffer;
  std::optional<std::size_t> carriageReturnIndex;
  // buffer[searchFrom..] has not been searched for a line end yet.
  std::size_t searchFrom = 0;
public:
  std::vector<std::string> decode(std::string_view chunk);
  std::vector<std::string> flush();
};
struct ServerSentEvent {
  std::optional<std::string> event;
  std::string data;
};
class SSEDecoder {
  LineDecoder lines;
  std::optional<std::string> event;
  std::vector<std::string> data;
  std::vector<ServerSentEvent> consume(std::vector<std::string>&& input);
public:
  std::vector<ServerSentEvent> decode(std::string_view chunk);
  std::vector<ServerSentEvent> flush();
};
// Decoding and HTTP errors remain separate from the model reducer.
void dispatchSSE(const ServerSentEvent& event, const Json& headers,
                 const std::function<void(const Json&)>& onEvent);
} // namespace batbox::transport
