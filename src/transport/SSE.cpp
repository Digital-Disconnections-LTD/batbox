#include "transport/SSE.hpp"
#include "transport/Errors.hpp"

// C++ adaptation of SDK decoding algorithms; see THIRD_PARTY_NOTICES.md
// and third_party/notices/third-party/sdk/ for attribution and Apache-2.0 terms.

namespace batbox::transport {
namespace {
std::string decodeUTF8(std::string_view input) {
  std::string output;
  for (std::size_t i = 0; i < input.size();) {
    const auto start = i;
    const unsigned char lead = input[i++];
    if (lead < 0x80) { output += static_cast<char>(lead); continue; }
    int count = lead >= 0xc2 && lead <= 0xdf ? 1 : lead >= 0xe0 && lead <= 0xef ? 2 : lead >= 0xf0 && lead <= 0xf4 ? 3 : 0;
    if (!count) { output += "\xef\xbf\xbd"; continue; }
    bool valid = true;
    for (int n = 0; n < count; ++n) {
      if (i == input.size()) { valid = false; break; }
      const unsigned char byte = input[i];
      const unsigned char lower = n == 0 && lead == 0xe0 ? 0xa0 : n == 0 && lead == 0xf0 ? 0x90 : 0x80;
      const unsigned char upper = n == 0 && lead == 0xed ? 0x9f : n == 0 && lead == 0xf4 ? 0x8f : 0xbf;
      if (byte < lower || byte > upper) { valid = false; break; }
      ++i;
    }
    if (valid) output.append(input.substr(start,i-start));
    else output += "\xef\xbf\xbd";
  }
  if (output.starts_with("\xef\xbb\xbf")) output.erase(0,3);
  return output;
}
} // namespace
std::optional<Newline> findNewlineIndex(std::string_view buffer, std::optional<std::size_t> startIndex) {
  for (std::size_t i = startIndex.value_or(0); i < buffer.size(); ++i) {
    if (buffer[i] == '\n') return Newline{i,i+1,false};
    if (buffer[i] == '\r') return Newline{i,i+1,true};
  }
  return {};
}
std::vector<std::string> LineDecoder::decode(std::string_view chunk) {
  buffer.append(chunk);
  std::vector<std::string> result;
  while (const auto pattern = findNewlineIndex(buffer,carriageReturnIndex)) {
    if (pattern->carriage && !carriageReturnIndex) {
      carriageReturnIndex = pattern->index;
      continue;
    }
    if (carriageReturnIndex && (pattern->index != *carriageReturnIndex+1 || pattern->carriage)) {
      result.push_back(decodeUTF8(buffer.substr(0,*carriageReturnIndex-1)));
      buffer.erase(0,*carriageReturnIndex);
      carriageReturnIndex.reset();
      continue;
    }
    const auto endIndex = carriageReturnIndex ? pattern->preceding-1 : pattern->preceding;
    result.push_back(decodeUTF8(buffer.substr(0,endIndex)));
    buffer.erase(0,pattern->index);
    carriageReturnIndex.reset();
  }
  return result;
}
std::vector<std::string> LineDecoder::flush() {
  if (buffer.empty()) return {};
  return decode("\n");
}
std::vector<ServerSentEvent> SSEDecoder::consume(const std::vector<std::string>& input) {
  std::vector<ServerSentEvent> result;
  for (auto line : input) {
    if (line.ends_with('\r')) line.pop_back();
    if (line.empty()) {
      if ((!event || event->empty()) && data.empty()) continue;
      std::string joined;
      for (std::size_t i = 0; i < data.size(); ++i) { if (i) joined += '\n'; joined += data[i]; }
      result.push_back({event,joined,chunks});
      chunks.clear();
      event.reset(); data.clear();
      continue;
    }
    chunks.push_back(line);
    if (line.starts_with(':')) continue;
    const auto colon = line.find(':');
    const auto field = line.substr(0,colon);
    auto value = colon == std::string::npos ? "" : line.substr(colon+1);
    if (value.starts_with(' ')) value.erase(0,1);
    if (field == "event") event = value;
    else if (field == "data") data.push_back(value);
  }
  return result;
}
std::vector<ServerSentEvent> SSEDecoder::decode(std::string_view chunk) { return consume(lines.decode(chunk)); }
std::vector<ServerSentEvent> SSEDecoder::flush() { return consume(lines.flush()); }
void dispatchSSE(const ServerSentEvent& event, const Json& headers, const std::function<void(const Json&)>& onEvent) {
  if (event.event == "ping" || !event.event) return;
  if (event.event == "error") {
    Json value = value::parse(event.data,false);
    if (value.is_discarded() || value.is_null()) value = event.data;
    throw APIError({},std::move(value),headers);
  }
  if (event.data.empty()) return;
  Json value = value::parse(event.data);
  onEvent(value);
}
} // namespace batbox::transport
