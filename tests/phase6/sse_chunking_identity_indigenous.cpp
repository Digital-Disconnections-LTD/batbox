#include "transport/SSE.hpp"
#include <iostream>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

using batbox::Json;
namespace {
void check(bool condition, const std::string& what) {
  if (!condition) throw std::runtime_error("check failed: " + what);
}

namespace reference {
// The SSE line and event decoders as they were before the partial-line
// search offset and the in-place data values, kept to compare events.
std::string decodeUTF8(std::string_view input) {
  std::string output;
  std::size_t runStart = 0;
  for (std::size_t i = 0; i < input.size();) {
    const auto start = i;
    const unsigned char lead = input[i++];
    if (lead < 0x80) continue;
    int count = lead >= 0xc2 && lead <= 0xdf ? 1 : lead >= 0xe0 && lead <= 0xef ? 2 : lead >= 0xf0 && lead <= 0xf4 ? 3 : 0;
    bool valid = count != 0;
    for (int n = 0; valid && n < count; ++n) {
      if (i == input.size()) { valid = false; break; }
      const unsigned char byte = input[i];
      const unsigned char lower = n == 0 && lead == 0xe0 ? 0xa0 : n == 0 && lead == 0xf0 ? 0x90 : 0x80;
      const unsigned char upper = n == 0 && lead == 0xed ? 0x9f : n == 0 && lead == 0xf4 ? 0x8f : 0xbf;
      if (byte < lower || byte > upper) { valid = false; break; }
      ++i;
    }
    if (valid) continue;
    output.append(input.substr(runStart, start - runStart));
    output += "\xef\xbf\xbd";
    runStart = i;
  }
  if (runStart == 0 && !input.starts_with("\xef\xbb\xbf")) return std::string(input);
  output.append(input.substr(runStart));
  if (output.starts_with("\xef\xbb\xbf")) output.erase(0, 3);
  return output;
}
struct LineDecoder {
  std::string buffer;
  std::optional<std::size_t> carriageReturnIndex;
  std::vector<std::string> decode(std::string_view chunk) {
    buffer.append(chunk);
    std::vector<std::string> result;
    std::size_t start = 0;
    const std::string_view view(buffer);
    while (const auto pattern = batbox::transport::findNewlineIndex(view, carriageReturnIndex.value_or(start))) {
      if (pattern->carriage && !carriageReturnIndex) { carriageReturnIndex = pattern->index; continue; }
      if (carriageReturnIndex && (pattern->index != *carriageReturnIndex + 1 || pattern->carriage)) {
        result.push_back(decodeUTF8(view.substr(start, *carriageReturnIndex - 1 - start)));
        start = *carriageReturnIndex;
        carriageReturnIndex.reset();
        continue;
      }
      const auto endIndex = carriageReturnIndex ? pattern->preceding - 1 : pattern->preceding;
      result.push_back(decodeUTF8(view.substr(start, endIndex - start)));
      start = pattern->index;
      carriageReturnIndex.reset();
    }
    if (start) {
      buffer.erase(0, start);
      if (carriageReturnIndex) *carriageReturnIndex -= start;
    }
    return result;
  }
  std::vector<std::string> flush() { return buffer.empty() ? std::vector<std::string>{} : decode("\n"); }
};
struct SSEDecoder {
  LineDecoder lines;
  std::optional<std::string> event;
  std::vector<std::string> data;
  std::vector<std::pair<std::optional<std::string>, std::string>> consume(std::vector<std::string>&& input) {
    std::vector<std::pair<std::optional<std::string>, std::string>> result;
    for (auto& line : input) {
      if (line.ends_with('\r')) line.pop_back();
      if (line.empty()) {
        if ((!event || event->empty()) && data.empty()) continue;
        std::string joined;
        for (std::size_t i = 0; i < data.size(); ++i) { if (i) joined += '\n'; joined += data[i]; }
        result.emplace_back(std::move(event), std::move(joined));
        event.reset(); data.clear();
        continue;
      }
      const std::string_view kept = line;
      if (kept.starts_with(':')) continue;
      const auto colon = kept.find(':');
      const auto field = kept.substr(0, colon);
      auto value = colon == std::string_view::npos ? std::string_view{} : kept.substr(colon + 1);
      if (value.starts_with(' ')) value.remove_prefix(1);
      if (field == "event") event = std::string(value);
      else if (field == "data") data.emplace_back(value);
    }
    return result;
  }
};
} // namespace reference

using Events = std::vector<std::pair<std::optional<std::string>, std::string>>;
Events decodeCurrent(const std::string& stream, const std::vector<std::size_t>& cuts) {
  batbox::transport::SSEDecoder decoder;
  Events out;
  auto take = [&](std::vector<batbox::transport::ServerSentEvent> events) {
    for (auto& event : events) out.emplace_back(std::move(event.event), std::move(event.data));
  };
  std::size_t at = 0;
  for (const auto cut : cuts) { take(decoder.decode(std::string_view(stream).substr(at, cut - at))); at = cut; }
  take(decoder.decode(std::string_view(stream).substr(at)));
  take(decoder.flush());
  return out;
}
Events decodeReference(const std::string& stream) {
  reference::SSEDecoder decoder;
  Events out;
  for (auto& event : decoder.consume(decoder.lines.decode(stream))) out.push_back(std::move(event));
  for (auto& event : decoder.consume(decoder.lines.flush())) out.push_back(std::move(event));
  return out;
}

void sseMatchesReference() {
  std::mt19937 random(680);
  const std::vector<std::string> lines = {"event: message", "event: ping", "event:", "data: {\"a\":1}", "data:plain",
    "data: naïve 日本", "data", ": comment", "id: 7", "retry: 10", "data: " + std::string(5000, 'x'), "", "", ""};
  const std::vector<std::string> ends = {"\n", "\r\n", "\r"};
  for (int round = 0; round < 300; ++round) {
    std::string stream;
    for (auto n = random() % 40; n > 0; --n) stream += lines[random() % lines.size()] + ends[random() % ends.size()];
    if (random() % 4 == 0) stream += "data: tail without end";
    const auto expected = decodeReference(stream);
    check(decodeCurrent(stream, {}) == expected, "whole stream decodes the same");
    std::vector<std::size_t> bytes;
    for (std::size_t i = 1; i < stream.size(); ++i) bytes.push_back(i);
    check(decodeCurrent(stream, bytes) == expected, "byte-by-byte stream decodes the same");
    std::vector<std::size_t> cuts;
    for (std::size_t at = 0; stream.size() > 1;) {
      at += 1 + random() % 700;
      if (at >= stream.size()) break;
      cuts.push_back(at);
    }
    check(decodeCurrent(stream, cuts) == expected, "random chunks decode the same");
  }
}

} // namespace

int main() {
  sseMatchesReference();
  std::cout << "SSE decoding: events match the reference for whole, byte-by-byte and random chunks\n";
  return 0;
}
