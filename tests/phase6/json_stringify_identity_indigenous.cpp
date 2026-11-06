#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

using batbox::Json;
namespace value = batbox::value;

namespace reference {
// The serializer as it was before the single-buffer rewrite, kept verbatim so
// every byte the new one writes can be compared against it.
void encode(std::string& out, std::uint32_t cp) {
  if (cp < 0x80) out += static_cast<char>(cp);
  else if (cp < 0x800) { out += static_cast<char>(0xc0 | (cp >> 6)); out += static_cast<char>(0x80 | (cp & 63)); }
  else if (cp < 0x10000) { out += static_cast<char>(0xe0 | (cp >> 12)); out += static_cast<char>(0x80 | ((cp >> 6) & 63)); out += static_cast<char>(0x80 | (cp & 63)); }
  else { out += static_cast<char>(0xf0 | (cp >> 18)); out += static_cast<char>(0x80 | ((cp >> 12) & 63)); out += static_cast<char>(0x80 | ((cp >> 6) & 63)); out += static_cast<char>(0x80 | (cp & 63)); }
}
std::u16string utf16CodeUnits(std::string_view text) {
  std::u16string out;
  for (std::size_t i = 0; i < text.size();) {
    const auto first = static_cast<unsigned char>(text[i++]);
    std::uint32_t cp = first;
    int remaining = 0;
    if ((first & 0xe0) == 0xc0) { cp = first & 31; remaining = 1; }
    else if ((first & 0xf0) == 0xe0) { cp = first & 15; remaining = 2; }
    else if ((first & 0xf8) == 0xf0) { cp = first & 7; remaining = 3; }
    else if (first >= 0x80) throw std::invalid_argument("Invalid WTF-8");
    const auto width = remaining;
    while (remaining--) {
      if (i == text.size()) throw std::invalid_argument("Incomplete WTF-8");
      const auto next = static_cast<unsigned char>(text[i++]);
      if ((next & 0xc0) != 0x80) throw std::invalid_argument("Invalid WTF-8 continuation");
      cp = (cp << 6) | (next & 63);
    }
    if ((width == 1 && cp < 0x80) || (width == 2 && cp < 0x800) || (width == 3 && cp < 0x10000) || cp > 0x10ffff)
      throw std::invalid_argument("Invalid WTF-8 scalar");
    if (cp <= 0xffff) out += static_cast<char16_t>(cp);
    else { cp -= 0x10000; out += static_cast<char16_t>(0xd800 + (cp >> 10)); out += static_cast<char16_t>(0xdc00 + (cp & 1023)); }
  }
  return out;
}
std::string quote(std::string_view text) {
  const auto units = utf16CodeUnits(text);
  std::string out = "\"";
  constexpr char hex[] = "0123456789abcdef";
  for (std::size_t i = 0; i < units.size(); ++i) {
    std::uint32_t cp = units[i];
    if (cp == '"' || cp == '\\') { out += '\\'; out += static_cast<char>(cp); continue; }
    if (cp == '\b') { out += "\\b"; continue; }
    if (cp == '\f') { out += "\\f"; continue; }
    if (cp == '\n') { out += "\\n"; continue; }
    if (cp == '\r') { out += "\\r"; continue; }
    if (cp == '\t') { out += "\\t"; continue; }
    if (cp >= 0xd800 && cp <= 0xdbff && i + 1 < units.size() && units[i + 1] >= 0xdc00 && units[i + 1] <= 0xdfff)
      cp = 0x10000 + ((cp - 0xd800) << 10) + (units[++i] - 0xdc00);
    if (cp < 0x20 || (cp >= 0xd800 && cp <= 0xdfff)) {
      out += "\\u";
      for (int shift = 12; shift >= 0; shift -= 4) out += hex[(cp >> shift) & 15];
    } else encode(out, cp);
  }
  return out + '"';
}
std::string number(double n) { return std::isfinite(n) ? value::numberToString(n) : "null"; }
std::string stringify(const Json& v) {
  if (v.is_number()) return number(v.get<double>());
  if (v.is_array()) {
    std::string out = "[";
    for (const auto& item : v) { if (out.size() > 1) out += ','; out += stringify(item); }
    return out + ']';
  }
  if (v.is_object()) {
    const auto keys = value::objectKeys(v);
    std::string out = "{";
    for (const auto& key : keys) {
      if (out.size() > 1) out += ',';
      out += quote(key) + ':' + stringify(v.at(key));
    }
    return out + '}';
  }
  if (v.is_string()) return quote(v.get_ref<const std::string&>());
  return v.dump();
}
} // namespace reference

namespace {
int failures = 0;
void fail(const std::string& what) {
  if (++failures <= 20) std::cerr << "FAIL: " << what << '\n';
}
std::string hexOf(std::string_view text) {
  static constexpr char digits[] = "0123456789abcdef";
  std::string out;
  for (unsigned char c : text.substr(0, 64)) { out += digits[c >> 4]; out += digits[c & 15]; out += ' '; }
  return out;
}
template <class F>
std::pair<std::optional<std::string>, std::string> run(F&& f) {
  try { return {f(), ""}; }
  catch (const std::invalid_argument& error) { return {std::nullopt, error.what()}; }
}
void checkQuote(const std::string& text) {
  const auto expected = run([&] { return reference::quote(text); });
  const auto actual = run([&] { return value::quote(text); });
  if (expected != actual) fail("quote differs for bytes " + hexOf(text));
  std::string appended = "prefix";
  const auto appendedResult = run([&] { value::appendQuoted(appended, text); return appended; });
  if (expected.first && appendedResult.first != "prefix" + *expected.first) fail("appendQuoted differs for bytes " + hexOf(text));
}
void checkStringify(const Json& json, const std::string& label) {
  const auto expected = run([&] { return reference::stringify(json); });
  const auto actual = run([&] { return value::stringify(json); });
  if (expected != actual) fail("stringify differs: " + label);
}
std::string encodeWtf8(std::uint32_t cp) {
  std::string out;
  reference::encode(out, cp);
  return out;
}
std::string randomText(std::mt19937& random, std::size_t length) {
  static const std::vector<std::string> pieces = {
    "a", "Z", "0", " ", "~", "\x7f", "\"", "\\", "/", "\b", "\f", "\n", "\r", "\t", std::string(1, '\0'),
    "\x01", "\x1b", "\x1f", "é", "ß", "界", "€", "\xef\xbb\xbf", "👍", "👩‍💻", "😀",
    encodeWtf8(0xd800), encodeWtf8(0xdbff), encodeWtf8(0xdc00), encodeWtf8(0xdfff), encodeWtf8(0xd83d) + encodeWtf8(0xde00),
    "\u2028", "\u2029", "abcdefghijklmnop", "\"quoted\"", "C:\\path\\to",
  };
  std::string text;
  for (std::size_t i = 0; i < length; ++i) text += pieces[random() % pieces.size()];
  return text;
}
Json randomJson(std::mt19937& random, int depth) {
  const auto kind = depth <= 0 ? random() % 4 : random() % 6;
  switch (kind) {
    case 0: return randomText(random, random() % 24);
    case 1: {
      const double choices[] = {0.0, -0.0, 1.0, -1.5, 1e21, 1e-7, 123456789.0, 0.1, 3.14159, -2e-300, 9007199254740993.0};
      return choices[random() % (sizeof(choices) / sizeof(choices[0]))];
    }
    case 2: return random() % 2 == 0 ? Json(static_cast<std::int64_t>(random()) - 1000000) : Json(static_cast<std::uint64_t>(random()) * 4096);
    case 3: return random() % 3 == 0 ? Json(nullptr) : Json(random() % 2 == 0);
    case 4: {
      Json array = Json::array();
      for (auto n = random() % 6; n > 0; --n) array.push_back(randomJson(random, depth - 1));
      return array;
    }
    default: {
      Json object = Json::object();
      for (auto n = random() % 6; n > 0; --n) {
        std::string key;
        switch (random() % 4) {
          case 0: key = std::to_string(random() % 20); break;
          case 1: key = random() % 2 ? "01" : "4294967295"; break;
          default: key = randomText(random, random() % 6); break;
        }
        object[key] = randomJson(random, depth - 1);
      }
      return object;
    }
  }
}
} // namespace

int main() {
  std::vector<std::string> corpus = {
    "", "a", "plain ascii text", "exactly8", "sixteen chars!!!", std::string(4096, 'x'),
    "\"", "\\", "\"\\\"", "tab\there", "line\nbreak", "\r\n", "\b\f", "\x7f", std::string("nul\0byte", 8),
    "é", "naïve café", "界", "日本語", "👍🏽", "👩‍💻", "\u2028\u2029", "\xef\xbb\xbf" "bom",
    encodeWtf8(0xd800), encodeWtf8(0xdfff), encodeWtf8(0xdc00) + encodeWtf8(0xd800),
    encodeWtf8(0xd83d) + encodeWtf8(0xde00), encodeWtf8(0xd83d) + "👍", "👍" + encodeWtf8(0xde00),
    encodeWtf8(0xd83d) + encodeWtf8(0xd83d) + encodeWtf8(0xde00), encodeWtf8(0xd83d) + "x",
    "\x80", "abc\xc3", "abc\xe2\x82", "\xc0\x80", "\xe0\x80\x80", "\xf0\x80\x80\x80", "\xf4\x90\x80\x80",
    "\xf8\x88\x80\x80\x80", "valid then \xff", "\xc3(", "abcdefgh\xc3", encodeWtf8(0xd800) + "\xc3",
  };
  for (int c = 0; c < 0x80; ++c) {
    corpus.push_back(std::string(1, static_cast<char>(c)));
    corpus.push_back("12345678" + std::string(1, static_cast<char>(c)) + "abcdefgh");
    corpus.push_back(std::string(7, 'p') + std::string(1, static_cast<char>(c)));
  }
  for (std::uint32_t cp = 0x80; cp < 0x110000; cp += (cp < 0x3000 ? 1 : 97)) corpus.push_back(encodeWtf8(cp));
  std::mt19937 random(680);
  for (int i = 0; i < 20000; ++i) {
    auto text = randomText(random, random() % 40);
    if (i % 13 == 0 && !text.empty()) text[random() % text.size()] = static_cast<char>(0x80 + random() % 0x80);
    if (i % 17 == 0 && !text.empty()) text.resize(random() % text.size());
    corpus.push_back(std::move(text));
  }
  for (const auto& text : corpus) {
    checkQuote(text);
    checkStringify(Json(text), "string " + hexOf(text));
    checkStringify(Json{{text, 1}, {"0", text}}, "key " + hexOf(text));
  }

  for (int i = 0; i < 4000; ++i) checkStringify(randomJson(random, 5), "random tree " + std::to_string(i));
  checkStringify(Json::object(), "empty object");
  checkStringify(Json::array(), "empty array");
  checkStringify(Json{{"b", 1}, {"10", 2}, {"a", 3}, {"2", 4}, {"01", 5}, {"-1", 6}}, "mixed index keys");
  checkStringify(Json(std::numeric_limits<double>::quiet_NaN()), "nan");
  checkStringify(Json(std::numeric_limits<double>::infinity()), "infinity");
  checkStringify(Json::binary({1, 2, 3}), "binary");

  Json deep = "leaf";
  for (int i = 0; i < 400; ++i) deep = (i % 2 == 0) ? Json::array({deep, i}) : Json{{"level", i}, {"next", deep}};
  checkStringify(deep, "deep nesting");

  Json large = Json::array();
  for (int i = 0; i < 400; ++i) {
    Json message{{"role", i % 2 ? "assistant" : "user"}, {"content", Json::array()}};
    message["content"].push_back({{"type", "text"}, {"text", randomText(random, 200 + (i % 7) * 400)}});
    if (i % 3 == 0) message["content"].push_back({{"type", "tool_result"}, {"tool_use_id", "toolu_" + std::to_string(i)},
      {"content", std::string(30000, 'r') + "\n界\t\"end\""}});
    large.push_back(std::move(message));
  }
  checkStringify(large, "large conversation");

  const auto time = [&](auto&& serialize) {
    const auto start = std::chrono::steady_clock::now();
    std::size_t bytes = 0;
    for (int i = 0; i < 5; ++i) bytes += serialize(large).size();
    const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    return std::pair{elapsed / 5, bytes / 5};
  };
  const auto [before, size] = time(reference::stringify);
  const auto [after, _] = time([](const Json& v) { return value::stringify(v); });
  std::cout << "stringify " << size << " bytes: reference " << before << " ms, current " << after << " ms\n";

  if (failures) {
    std::cerr << failures << " mismatches\n";
    return 1;
  }
  std::cout << "json stringify identity: " << corpus.size() << " strings, 4000 random trees\n";
  return 0;
}
