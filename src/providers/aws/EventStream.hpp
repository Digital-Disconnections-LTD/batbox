#pragma once
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace batbox::providers::aws {
// application/vnd.amazon.eventstream: 12-byte prelude (total length, headers
// length, prelude CRC32), typed headers, payload, then a CRC32 of the whole message.
struct EventStreamMessage {
  // Header values are rendered as text: strings as-is, integers in decimal,
  // booleans as true/false, bytes in base64, UUIDs in hex, timestamps in epoch milliseconds.
  std::map<std::string,std::string> headers;
  std::string payload;
  std::string header(const std::string& name) const;
};
struct EventStreamError : std::runtime_error {
  using std::runtime_error::runtime_error;
};
class EventStreamDecoder {
  std::string buffer;
public:
  static constexpr std::uint32_t maxMessageBytes = 24u*1024u*1024u;
  // Returns every message completed by this chunk. Throws EventStreamError on corrupt framing.
  std::vector<EventStreamMessage> decode(std::string_view chunk);
  bool pending() const { return !buffer.empty(); }
  std::size_t pendingBytes() const { return buffer.size(); }
};
EventStreamMessage parseEventStreamMessage(std::string_view message);
} // namespace batbox::providers::aws
