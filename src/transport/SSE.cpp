#include "transport/SSE.hpp"
#include "transport/Errors.hpp"
#include <algorithm>
#include <utility>

// C++ adaptation of SDK decoding algorithms; see THIRD_PARTY_NOTICES.md
// and third_party/notices/third-party/sdk/ for attribution and Apache-2.0 terms.

namespace batbox::transport {
namespace {
std::string decodeUTF8(std::string_view input) {
  // Valid runs are appended in one piece; only malformed sequences are
  // replaced byte by byte.
  std::string output;
  output.reserve(input.size());
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
    output.append(input.substr(runStart,start-runStart));
    output += "\xef\xbf\xbd";
    runStart = i;
  }
  if (runStart == 0 && !input.starts_with("\xef\xbb\xbf")) return std::string(input);
  output.append(input.substr(runStart));
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
  // Consumed bytes are dropped once per chunk instead of once per line.
  std::size_t start = 0;
  const std::string_view view(buffer);
  // The first search skips bytes an earlier chunk already searched.
  auto searchStart = [&] {
    const auto from = carriageReturnIndex.value_or(start);
    return std::max(from, std::exchange(searchFrom, 0));
  };
  while (const auto pattern = findNewlineIndex(view,searchStart())) {
    if (pattern->carriage && !carriageReturnIndex) {
      carriageReturnIndex = pattern->index;
      continue;
    }
    if (carriageReturnIndex && (pattern->index != *carriageReturnIndex+1 || pattern->carriage)) {
      result.push_back(decodeUTF8(view.substr(start,*carriageReturnIndex-1-start)));
      start = *carriageReturnIndex;
      carriageReturnIndex.reset();
      continue;
    }
    const auto endIndex = carriageReturnIndex ? pattern->preceding-1 : pattern->preceding;
    result.push_back(decodeUTF8(view.substr(start,endIndex-start)));
    start = pattern->index;
    carriageReturnIndex.reset();
  }
  searchFrom = view.size() - start;
  if (start) {
    buffer.erase(0,start);
    if (carriageReturnIndex) *carriageReturnIndex -= start;
  }
  return result;
}
std::vector<std::string> LineDecoder::flush() {
  if (buffer.empty()) return {};
  return decode("\n");
}
std::vector<ServerSentEvent> SSEDecoder::consume(std::vector<std::string>&& input) {
  std::vector<ServerSentEvent> result;
  for (auto& line : input) {
    if (line.ends_with('\r')) line.pop_back();
    if (line.empty()) {
      if ((!event || event->empty()) && data.empty()) continue;
      std::string joined;
      if (data.size() == 1) joined = std::move(data.front());
      else for (std::size_t i = 0; i < data.size(); ++i) { if (i) joined += '\n'; joined += data[i]; }
      result.push_back({std::move(event),std::move(joined)});
      event.reset(); data.clear();
      continue;
    }
    const std::string_view kept = line;
    if (kept.starts_with(':')) continue;
    const auto colon = kept.find(':');
    const auto field = kept.substr(0,colon);
    auto value = colon == std::string_view::npos ? std::string_view{} : kept.substr(colon+1);
    if (value.starts_with(' ')) value.remove_prefix(1);
    if (field == "event") event = std::string(value);
    else if (field == "data") {
      // The value is the tail of line; keep it without another allocation.
      line.erase(0, static_cast<std::size_t>(value.data() - line.data()));
      data.push_back(std::move(line));
    }
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
