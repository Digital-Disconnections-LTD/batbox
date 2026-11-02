#include "providers/aws/EventStream.hpp"
#include "providers/cloud/Crypto.hpp"
#include <string>

namespace batbox::providers::aws {
namespace {
std::uint32_t readU32(std::string_view bytes, std::size_t at) {
  return (std::uint32_t(static_cast<unsigned char>(bytes[at])) << 24) | (std::uint32_t(static_cast<unsigned char>(bytes[at+1])) << 16) |
         (std::uint32_t(static_cast<unsigned char>(bytes[at+2])) << 8) | std::uint32_t(static_cast<unsigned char>(bytes[at+3]));
}
std::uint64_t readBigEndian(std::string_view bytes, std::size_t at, std::size_t width) {
  std::uint64_t value = 0;
  for (std::size_t i = 0; i < width; ++i) value = (value << 8) | static_cast<unsigned char>(bytes[at+i]);
  return value;
}
std::int64_t signedValue(std::uint64_t raw, std::size_t width) {
  if (width < 8 && (raw >> (width*8-1)) & 1) raw |= ~((std::uint64_t(1) << (width*8))-1);
  return static_cast<std::int64_t>(raw);
}
} // namespace
std::string EventStreamMessage::header(const std::string& name) const {
  const auto it = headers.find(name);
  return it == headers.end() ? std::string() : it->second;
}
EventStreamMessage parseEventStreamMessage(std::string_view message) {
  if (message.size() < 16) throw EventStreamError("Event stream message is shorter than its prelude");
  const auto total = readU32(message,0);
  const auto headersLength = readU32(message,4);
  if (total != message.size()) throw EventStreamError("Event stream message length does not match its prelude");
  if (cloud::crc32(message.substr(0,8)) != readU32(message,8)) throw EventStreamError("Event stream prelude CRC mismatch");
  if (cloud::crc32(message.substr(0,total-4)) != readU32(message,total-4)) throw EventStreamError("Event stream message CRC mismatch");
  if (headersLength > total-16) throw EventStreamError("Event stream headers overrun the message");
  EventStreamMessage result;
  const auto headers = message.substr(12,headersLength);
  std::size_t at = 0;
  auto need = [&](std::size_t bytes) {
    if (at+bytes > headers.size()) throw EventStreamError("Event stream header is truncated");
  };
  while (at < headers.size()) {
    need(1);
    const auto nameLength = static_cast<unsigned char>(headers[at++]);
    need(nameLength+1);
    std::string name(headers.substr(at,nameLength));
    at += nameLength;
    const auto type = static_cast<unsigned char>(headers[at++]);
    std::string text;
    switch (type) {
      case 0: text = "true"; break;
      case 1: text = "false"; break;
      case 2: case 3: case 4: case 5: {
        const std::size_t width = type == 2 ? 1 : type == 3 ? 2 : type == 4 ? 4 : 8;
        need(width);
        text = std::to_string(signedValue(readBigEndian(headers,at,width),width));
        at += width;
        break;
      }
      case 6: case 7: {
        need(2);
        const auto length = static_cast<std::size_t>(readBigEndian(headers,at,2));
        at += 2;
        need(length);
        text = type == 7 ? std::string(headers.substr(at,length)) : cloud::base64Encode(headers.substr(at,length));
        at += length;
        break;
      }
      case 8: need(8); text = std::to_string(signedValue(readBigEndian(headers,at,8),8)); at += 8; break;
      case 9: need(16); text = cloud::toHex(headers.substr(at,16)); at += 16; break;
      default: throw EventStreamError("Event stream header has unknown value type "+std::to_string(type));
    }
    result.headers[std::move(name)] = std::move(text);
  }
  result.payload = std::string(message.substr(12+headersLength,total-16-headersLength));
  return result;
}
std::vector<EventStreamMessage> EventStreamDecoder::decode(std::string_view chunk) {
  buffer.append(chunk);
  std::vector<EventStreamMessage> messages;
  std::size_t offset = 0;
  while (buffer.size()-offset >= 12) {
    const std::string_view view(buffer);
    const auto total = readU32(view,offset);
    if (cloud::crc32(view.substr(offset,8)) != readU32(view,offset+8)) throw EventStreamError("Event stream prelude CRC mismatch");
    if (total < 16 || total > maxMessageBytes) throw EventStreamError("Event stream message length is out of range");
    if (buffer.size()-offset < total) break;
    messages.push_back(parseEventStreamMessage(view.substr(offset,total)));
    offset += total;
  }
  buffer.erase(0,offset);
  return messages;
}
} // namespace batbox::providers::aws
